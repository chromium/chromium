// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/offline_page_model_taskified.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/add_page_task.h"
#include "components/offline_pages/core/model/cleanup_visuals_task.h"
#include "components/offline_pages/core/model/delete_page_task.h"
#include "components/offline_pages/core/model/get_pages_task.h"
#include "components/offline_pages/core/model/get_visuals_task.h"
#include "components/offline_pages/core/model/mark_page_accessed_task.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/model/persistent_page_consistency_check_task.h"
#include "components/offline_pages/core/model/startup_maintenance_task.h"
#include "components/offline_pages/core/model/store_visuals_task.h"
#include "components/offline_pages/core/model/update_publish_id_task.h"
#include "components/offline_pages/core/model/visuals_availability_task.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_archive_publisher.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "url/gurl.h"

namespace offline_pages {

using ArchiverResult = OfflinePageArchiver::ArchiverResult;
using ClearStorageResult = ClearStorageTask::ClearStorageResult;

namespace {

void WrapInMultipleItemsCallback(MultipleOfflineIdCallback callback,
                                 const MultipleOfflinePageItemResult& pages) {
  std::vector<int64_t> results;
  for (const auto& page : pages) {
    results.push_back(page.offline_id);
  }
  std::move(callback).Run(results);
}

SavePageResult ArchiverResultToSavePageResult(ArchiverResult archiver_result) {
  switch (archiver_result) {
    case ArchiverResult::SUCCESSFULLY_CREATED:
      return SavePageResult::SUCCESS;
    case ArchiverResult::ERROR_DEVICE_FULL:
      return SavePageResult::DEVICE_FULL;
    case ArchiverResult::ERROR_CONTENT_UNAVAILABLE:
      return SavePageResult::CONTENT_UNAVAILABLE;
    case ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED:
      return SavePageResult::ARCHIVE_CREATION_FAILED;
    case ArchiverResult::ERROR_CANCELED:
      return SavePageResult::CANCELLED;
    case ArchiverResult::ERROR_SKIPPED:
      return SavePageResult::SKIPPED;
    case ArchiverResult::ERROR_DIGEST_CALCULATION_FAILED:
      return SavePageResult::DIGEST_CALCULATION_FAILED;
  }
  NOTREACHED_IN_MIGRATION();
  return SavePageResult::CONTENT_UNAVAILABLE;
}

SavePageResult AddPageResultToSavePageResult(AddPageResult add_page_result) {
  switch (add_page_result) {
    case AddPageResult::SUCCESS:
      return SavePageResult::SUCCESS;
    case AddPageResult::ALREADY_EXISTS:
      return SavePageResult::ALREADY_EXISTS;
    case AddPageResult::STORE_FAILURE:
      return SavePageResult::STORE_FAILURE;
  }
  NOTREACHED_IN_MIGRATION();
  return SavePageResult::STORE_FAILURE;
}

void ReportSavedPagesCount(MultipleOfflinePageItemCallback callback,
                           const MultipleOfflinePageItemResult& all_items) {
  std::move(callback).Run(all_items);
}

void OnUpdateFilePathDone(PublishPageCallback publish_done_callback,
                          const base::FilePath& new_file_path,
                          SavePageResult result,
                          bool update_file_result) {
  if (update_file_result) {
    std::move(publish_done_callback).Run(new_file_path, result);
    return;
  }
  // If the file path wasn't updated successfully, just invoke the callback with
  // store failure.
  std::move(publish_done_callback)
      .Run(new_file_path, SavePageResult::STORE_FAILURE);
}

}  // namespace

// static
constexpr base::TimeDelta OfflinePageModelTaskified::kMaintenanceTasksDelay;

// static
constexpr base::TimeDelta OfflinePageModelTaskified::kClearStorageInterval;

OfflinePageModelTaskified::OfflinePageModelTaskified(
    std::unique_ptr<OfflinePageMetadataStore> store,
    std::unique_ptr<ArchiveManager> archive_manager,
    std::unique_ptr<OfflinePageArchivePublisher> archive_publisher,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : store_(std::move(store)),
      archive_manager_(std::move(archive_manager)),
      archive_publisher_(std::move(archive_publisher)),
      task_queue_(this),
      skip_clearing_original_url_for_testing_(false),
      skip_maintenance_tasks_for_testing_(false),
      task_runner_(task_runner) {
  DCHECK_LT(kMaintenanceTasksDelay, OfflinePageMetadataStore::kClosingDelay);
  CreateArchivesDirectoryIfNeeded();
}

OfflinePageModelTaskified::~OfflinePageModelTaskified() = default;

void OfflinePageModelTaskified::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OfflinePageModelTaskified::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OfflinePageModelTaskified::OnTaskQueueIsIdle() {}

void OfflinePageModelTaskified::SavePage(
    const SavePageParams& save_page_params,
    std::unique_ptr<OfflinePageArchiver> archiver,
    content::WebContents* web_contents,
    SavePageCallback callback) {
  // Skip saving the page that is not intended to be saved, like local file
  // page.
  if (!OfflinePageModel::CanSaveURL(save_page_params.url)) {
    InformSavePageDone(std::move(callback), SavePageResult::SKIPPED,
                       save_page_params.client_id, kInvalidOfflineId);
    return;
  }

  // The web contents is not available if archiver is not created and passed.
  if (!archiver) {
    InformSavePageDone(std::move(callback), SavePageResult::CONTENT_UNAVAILABLE,
                       save_page_params.client_id, kInvalidOfflineId);
    return;
  }

  // If we already have an offline id, use it.  If not, generate one.
  int64_t offline_id = save_page_params.proposed_offline_id;
  if (offline_id == kInvalidOfflineId) {
    offline_id = store_utils::GenerateOfflineId();
  }

  OfflinePageArchiver::CreateArchiveParams create_archive_params(
      save_page_params.client_id.name_space);
  // If the page is being saved in the background, we should try to remove the
  // popup overlay that obstructs viewing the normal content.
  create_archive_params.remove_popup_overlay = save_page_params.is_background;

  // Save directly to public location if on-the-fly enabled.
  //
  // TODO(crbug.com/40642718): We would like to skip renaming the file if
  // streaming the file directly to it's end location. Knowing the file path or
  // name before calling the archiver would make this possible.
  base::FilePath save_file_dir =
      GetArchiveDirectory(save_page_params.client_id.name_space);

  // Note: the archiver instance must be kept alive until the final callback
  // coming from it takes place.
  OfflinePageArchiver* raw_archiver = archiver.get();
  raw_archiver->CreateArchive(
      save_file_dir, create_archive_params, web_contents,
      base::BindOnce(&OfflinePageModelTaskified::OnCreateArchiveDone,
                     weak_ptr_factory_.GetWeakPtr(), save_page_params,
                     offline_id, OfflineTimeNow(), std::move(archiver),
                     std::move(callback)));
}

void OfflinePageModelTaskified::AddPage(const OfflinePageItem& page,
                                        AddPageCallback callback) {
  auto task = std::make_unique<AddPageTask>(
      store_.get(), page,
      base::BindOnce(&OfflinePageModelTaskified::OnAddPageDone,
                     weak_ptr_factory_.GetWeakPtr(), page,
                     std::move(callback)));
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::MarkPageAccessed(int64_t offline_id) {
  auto task = std::make_unique<MarkPageAccessedTask>(store_.get(), offline_id,
                                                     OfflineTimeNow());
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::DeletePagesWithCriteria(
    const PageCriteria& criteria,
    DeletePageCallback callback) {
  task_queue_.AddTask(DeletePageTask::CreateTaskWithCriteria(
      store_.get(), criteria,
      base::BindOnce(&OfflinePageModelTaskified::OnDeleteDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void OfflinePageModelTaskified::DeleteCachedPagesByURLPredicate(
    const UrlPredicate& predicate,
    DeletePageCallback callback) {
  auto task = DeletePageTask::CreateTaskMatchingUrlPredicateForCachedPages(
      store_.get(),
      base::BindOnce(&OfflinePageModelTaskified::OnDeleteDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      predicate);
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::GetAllPages(
    MultipleOfflinePageItemCallback callback) {
  GetPagesWithCriteria(PageCriteria(), base::BindOnce(&ReportSavedPagesCount,
                                                      std::move(callback)));
  ScheduleMaintenanceTasks();
}

void OfflinePageModelTaskified::GetPageByOfflineId(
    int64_t offline_id,
    SingleOfflinePageItemCallback callback) {
  PageCriteria criteria;
  criteria.offline_ids = std::vector<int64_t>{offline_id};
  // Adapt multiple result to single result callback.
  auto wrapped_callback = base::BindOnce(
      [](SingleOfflinePageItemCallback callback,
         const std::vector<OfflinePageItem>& pages) {
        std::move(callback).Run(pages.empty() ? nullptr : &pages[0]);
      },
      std::move(callback));
  GetPagesWithCriteria(criteria, std::move(wrapped_callback));
}

void OfflinePageModelTaskified::GetPagesWithCriteria(
    const PageCriteria& criteria,
    MultipleOfflinePageItemCallback callback) {
  task_queue_.AddTask(std::make_unique<GetPagesTask>(store_.get(), criteria,
                                                     std::move(callback)));
}

void OfflinePageModelTaskified::GetOfflineIdsForClientId(
    const ClientId& client_id,
    MultipleOfflineIdCallback callback) {
  // We're currently getting offline IDs by querying offline items based on
  // client ids, and then extract the offline IDs from the items. This is fine
  // since we're not expecting many pages with the same client ID.
  PageCriteria criteria;
  criteria.client_ids = std::vector<ClientId>{client_id};
  GetPagesWithCriteria(criteria, base::BindOnce(&WrapInMultipleItemsCallback,
                                                std::move(callback)));
}

void OfflinePageModelTaskified::StoreThumbnail(int64_t offline_id,
                                               std::string thumbnail) {
  task_queue_.AddTask(StoreVisualsTask::MakeStoreThumbnailTask(
      store_.get(), offline_id, std::move(thumbnail),
      base::BindOnce(&OfflinePageModelTaskified::OnStoreThumbnailDone,
                     weak_ptr_factory_.GetWeakPtr(), offline_id)));
}

void OfflinePageModelTaskified::StoreFavicon(int64_t offline_id,
                                             std::string favicon) {
  task_queue_.AddTask(StoreVisualsTask::MakeStoreFaviconTask(
      store_.get(), offline_id, std::move(favicon),
      base::BindOnce(&OfflinePageModelTaskified::OnStoreFaviconDone,
                     weak_ptr_factory_.GetWeakPtr(), offline_id)));
}

void OfflinePageModelTaskified::GetVisualsByOfflineId(
    int64_t offline_id,
    base::OnceCallback<void(std::unique_ptr<OfflinePageVisuals>)> callback) {
  task_queue_.AddTask(std::make_unique<GetVisualsTask>(store_.get(), offline_id,
                                                       std::move(callback)));
}

void OfflinePageModelTaskified::GetVisualsAvailability(
    int64_t offline_id,
    base::OnceCallback<void(VisualsAvailability)> callback) {
  task_queue_.AddTask(std::make_unique<VisualsAvailabilityTask>(
      store_.get(), offline_id, std::move(callback)));
}

const base::FilePath& OfflinePageModelTaskified::GetArchiveDirectory(
    const std::string& name_space) const {
  if (GetPolicy(name_space).lifetime_type == LifetimeType::TEMPORARY) {
    return archive_manager_->GetTemporaryArchivesDir();
  }
  return archive_manager_->GetPrivateArchivesDir();
}

bool OfflinePageModelTaskified::IsArchiveInInternalDir(
    const base::FilePath& file_path) const {
  DCHECK(!file_path.empty());

  // TODO(jianli): Update this once persistent archives are moved into the
  // public directory.
  return archive_manager_->GetTemporaryArchivesDir().IsParent(file_path) ||
         archive_manager_->GetPrivateArchivesDir().IsParent(file_path);
}

OfflineEventLogger* OfflinePageModelTaskified::GetLogger() {
  return &offline_event_logger_;
}

void OfflinePageModelTaskified::InformSavePageDone(SavePageCallback callback,
                                                   SavePageResult result,
                                                   const ClientId& client_id,
                                                   int64_t offline_id) {
  if (result == SavePageResult::ARCHIVE_CREATION_FAILED)
    CreateArchivesDirectoryIfNeeded();
  if (!callback.is_null())
    std::move(callback).Run(result, offline_id);
}

void OfflinePageModelTaskified::OnCreateArchiveDone(
    const SavePageParams& save_page_params,
    int64_t offline_id,
    base::Time start_time,
    std::unique_ptr<OfflinePageArchiver> archiver,
    SavePageCallback callback,
    ArchiverResult archiver_result,
    const GURL& saved_url,
    const base::FilePath& file_path,
    const std::u16string& title,
    int64_t file_size,
    const std::string& digest) {
  if (archiver_result != ArchiverResult::SUCCESSFULLY_CREATED) {
    SavePageResult result = ArchiverResultToSavePageResult(archiver_result);
    InformSavePageDone(std::move(callback), result, save_page_params.client_id,
                       offline_id);
    return;
  }
  if (save_page_params.url != saved_url) {
    DVLOG(1) << "Saved URL does not match requested URL.";
    InformSavePageDone(std::move(callback), SavePageResult::INCORRECT_URL,
                       save_page_params.client_id, offline_id);
    return;
  }

  OfflinePageItem offline_page(saved_url, offline_id,
                               save_page_params.client_id, file_path, file_size,
                               start_time);
  offline_page.title = title;
  offline_page.digest = digest;
  offline_page.request_origin = save_page_params.request_origin;
  // Don't record the original URL if it is identical to the final URL. This is
  // because some websites might route the redirect finally back to itself upon
  // the completion of certain action, i.e., authentication, in the middle.
  if (skip_clearing_original_url_for_testing_ ||
      save_page_params.original_url != offline_page.url) {
    offline_page.original_url_if_different = save_page_params.original_url;
  }

  if (GetPolicy(offline_page.client_id.name_space).lifetime_type ==
      LifetimeType::PERSISTENT) {
    // If the user intentionally downloaded the page (aka it belongs to a
    // persistent namespace), move it to a public place.
    archive_publisher_->PublishArchive(
        offline_page, task_runner_,
        base::BindOnce(&OfflinePageModelTaskified::PublishArchiveDone,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       OfflineTimeNow()));
    return;
  }

  // For pages that we download on the user's behalf, we keep them in an
  // internal chrome directory, and add them here to the OfflinePageModel
  // database.
  AddPage(offline_page,
          base::BindOnce(&OfflinePageModelTaskified::OnAddPageForSavePageDone,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         offline_page, OfflineTimeNow()));
  // Note: If the archiver instance ownership was not transferred, it will be
  // deleted here.
}

void OfflinePageModelTaskified::PublishArchiveDone(
    SavePageCallback callback,
    base::Time publish_start_time,
    const OfflinePageItem& offline_page,
    PublishArchiveResult publish_results) {
  if (publish_results.move_result != SavePageResult::SUCCESS) {
    InformSavePageDone(std::move(callback), publish_results.move_result,
                       offline_page.client_id, offline_page.offline_id);
    return;
  }

  const base::Time add_page_start_time = OfflineTimeNow();
  OfflinePageItem page = offline_page;
  page.file_path = publish_results.id.new_file_path;
  page.system_download_id = publish_results.id.download_id;

  AddPage(page,
          base::BindOnce(&OfflinePageModelTaskified::OnAddPageForSavePageDone,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         page, add_page_start_time));
}

void OfflinePageModelTaskified::PublishInternalArchive(
    const OfflinePageItem& offline_page,
    PublishPageCallback publish_done_callback) {
  archive_publisher_->PublishArchive(
      offline_page, task_runner_,
      base::BindOnce(&OfflinePageModelTaskified::PublishInternalArchiveDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(publish_done_callback)));
}

void OfflinePageModelTaskified::PublishInternalArchiveDone(
    PublishPageCallback publish_done_callback,
    const OfflinePageItem& offline_page,
    PublishArchiveResult publish_results) {
  SavePageResult result = publish_results.move_result;
  // Call the callback with success == false if we failed to move the page.
  if (result != SavePageResult::SUCCESS) {
    std::move(publish_done_callback)
        .Run(publish_results.id.new_file_path, result);
    return;
  }

  // Update the OfflinePageModel with the new location for the page, which is
  // found in move_results.id.new_file_path, and with the download ID found at
  // move_results.id.download_id.  Return the updated offline_page to the
  // callback.
  auto task = std::make_unique<UpdatePublishIdTask>(
      store_.get(), offline_page.offline_id, publish_results.id,
      base::BindOnce(&OnUpdateFilePathDone, std::move(publish_done_callback),
                     publish_results.id.new_file_path, result));
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::OnAddPageForSavePageDone(
    SavePageCallback callback,
    const OfflinePageItem& page_attempted,
    base::Time add_page_start_time,
    AddPageResult add_page_result,
    int64_t offline_id) {
  SavePageResult save_page_result =
      AddPageResultToSavePageResult(add_page_result);
  InformSavePageDone(std::move(callback), save_page_result,
                     page_attempted.client_id, offline_id);
  if (save_page_result == SavePageResult::SUCCESS) {
    // TODO(romax): Just keep the same with logic in OPMImpl (which was wrong).
    // This should be fixed once we have the new strategy for clearing pages.
    if (GetPolicy(page_attempted.client_id.name_space).pages_allowed_per_url !=
        kUnlimitedPages) {
      RemovePagesMatchingUrlAndNamespace(page_attempted);
    }
    offline_event_logger_.RecordPageSaved(page_attempted.client_id.name_space,
                                          page_attempted.url.spec(),
                                          page_attempted.offline_id);
  }
  ScheduleMaintenanceTasks();
}

void OfflinePageModelTaskified::OnAddPageDone(const OfflinePageItem& page,
                                              AddPageCallback callback,
                                              AddPageResult result) {
  std::move(callback).Run(result, page.offline_id);
  if (result == AddPageResult::SUCCESS) {
    for (Observer& observer : observers_) {
      observer.OfflinePageAdded(this, page);
    }
  }
}

void OfflinePageModelTaskified::OnDeleteDone(
    DeletePageCallback callback,
    DeletePageResult result,
    const std::vector<OfflinePageItem>& deleted_items) {
  std::vector<PublishedArchiveId> publish_ids;

  // Notify observers and run callback.
  for (const auto& item : deleted_items) {
    offline_event_logger_.RecordPageDeleted(item.offline_id);
    for (Observer& observer : observers_) {
      observer.OfflinePageDeleted(item);
    }

    publish_ids.emplace_back(item.system_download_id, item.file_path);
  }

  // Remove the page from the system download manager. We don't need to wait for
  // completion before calling the delete page callback.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OfflinePageModelTaskified::Unpublish,
                                archive_publisher_->GetWeakPtr(), publish_ids));

  if (!callback.is_null()) {
    std::move(callback).Run(result);
  }
}

void OfflinePageModelTaskified::OnStoreThumbnailDone(int64_t offline_id,
                                                     bool success,
                                                     std::string thumbnail) {
  if (success) {
    for (Observer& observer : observers_) {
      observer.ThumbnailAdded(this, offline_id, thumbnail);
    }
  }
}

void OfflinePageModelTaskified::OnStoreFaviconDone(int64_t offline_id,
                                                   bool success,
                                                   std::string favicon) {
  if (success) {
    for (Observer& observer : observers_) {
      observer.FaviconAdded(this, offline_id, favicon);
    }
  }
}

void OfflinePageModelTaskified::Unpublish(
    base::WeakPtr<OfflinePageArchivePublisher> publisher,
    const std::vector<PublishedArchiveId>& publish_ids) {
  if (publisher && !publish_ids.empty()) {
    publisher->UnpublishArchives(publish_ids);
  }
}

void OfflinePageModelTaskified::ScheduleMaintenanceTasks() {
  if (skip_maintenance_tasks_for_testing_) {
    return;
  }
  // If not enough time has passed, don't queue maintenance tasks.
  base::Time now = OfflineTimeNow();
  if (now - last_maintenance_tasks_schedule_time_ < kClearStorageInterval) {
    return;
  }

  bool first_run = last_maintenance_tasks_schedule_time_.is_null();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OfflinePageModelTaskified::RunMaintenanceTasks,
                     weak_ptr_factory_.GetWeakPtr(), now, first_run),
      kMaintenanceTasksDelay);

  last_maintenance_tasks_schedule_time_ = now;
}

void OfflinePageModelTaskified::RunMaintenanceTasks(base::Time now,
                                                    bool first_run) {
  DCHECK(!skip_maintenance_tasks_for_testing_);
  // If this is the first run of this session, enqueue the startup maintenance
  // task, including consistency checks, legacy archive directory cleaning and
  // reporting storage usage UMA.
  if (first_run) {
    task_queue_.AddTask(std::make_unique<StartupMaintenanceTask>(
        store_.get(), archive_manager_.get()));

    task_queue_.AddTask(std::make_unique<CleanupVisualsTask>(
        store_.get(), OfflineTimeNow(), base::DoNothing()));
  }

  // TODO(crbug.com/40572659) This might need a better execution plan.
  task_queue_.AddTask(std::make_unique<PersistentPageConsistencyCheckTask>(
      store_.get(), archive_manager_.get(), now,
      base::BindOnce(
          &OfflinePageModelTaskified::OnPersistentPageConsistencyCheckDone,
          weak_ptr_factory_.GetWeakPtr())));
}

void OfflinePageModelTaskified::OnPersistentPageConsistencyCheckDone(
    bool success,
    const std::vector<PublishedArchiveId>& ids_of_deleted_pages) {
  Unpublish(archive_publisher_->GetWeakPtr(), ids_of_deleted_pages);
}

void OfflinePageModelTaskified::RemovePagesMatchingUrlAndNamespace(
    const OfflinePageItem& page) {
  auto task = DeletePageTask::CreateTaskDeletingForPageLimit(
      store_.get(),
      base::BindOnce(&OfflinePageModelTaskified::OnDeleteDone,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()),
      page);
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::CreateArchivesDirectoryIfNeeded() {
  // No callback is required here.
  // TODO(romax): Remove the callback from the interface once the other
  // consumers of this API can also drop the callback.
  archive_manager_->EnsureArchivesDirCreated(base::DoNothing());
}

}  // namespace offline_pages
