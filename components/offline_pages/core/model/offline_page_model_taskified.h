// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_MODEL_TASKIFIED_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_MODEL_TASKIFIED_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/offline_pages/core/model/clear_storage_task.h"
#include "components/offline_pages/core/offline_page_archive_publisher.h"
#include "components/offline_pages/core/offline_page_archiver.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_model_event_logger.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/task/task_queue.h"

class GURL;
namespace base {
class FilePath;
class SequencedTaskRunner;
class Time;
class TimeDelta;
}  // namespace base

namespace offline_pages {

struct ClientId;
struct OfflinePageItem;

class ArchiveManager;
class OfflinePageArchivePublisher;
class OfflinePageArchiver;
class OfflinePageMetadataStore;

// Implementaion of OfflinePageModel, which is a service for saving pages
// offline. It's an entry point to get information about Offline Pages and the
// base of related Offline Pages features.
// It owns a database which stores offline metadata, and uses TaskQueue for
// executing various tasks, including database operation or other process that
// needs to run on a background thread.
class OfflinePageModelTaskified : public OfflinePageModel,
                                  public TaskQueue::Delegate {
 public:
  // Delay between the scheduling and actual running of maintenance tasks. To
  // not cause the re-opening of the metadata store this delay should be kept
  // smaller than OfflinePageMetadataStore::kClosingDelay.
  static constexpr base::TimeDelta kMaintenanceTasksDelay = base::Seconds(10);

  // Minimum delay between runs of maintenance tasks during a Chrome session.
  static constexpr base::TimeDelta kClearStorageInterval = base::Minutes(30);

  OfflinePageModelTaskified(
      std::unique_ptr<OfflinePageMetadataStore> store,
      std::unique_ptr<ArchiveManager> archive_manager,
      std::unique_ptr<OfflinePageArchivePublisher> archive_publisher,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  OfflinePageModelTaskified(const OfflinePageModelTaskified&) = delete;
  OfflinePageModelTaskified& operator=(const OfflinePageModelTaskified&) =
      delete;

  ~OfflinePageModelTaskified() override;

  // TaskQueue::Delegate implementation.
  void OnTaskQueueIsIdle() override;

  // OfflinePageModel implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void SavePage(const SavePageParams& save_page_params,
                std::unique_ptr<OfflinePageArchiver> archiver,
                content::WebContents* web_contents,
                SavePageCallback callback) override;
  void AddPage(const OfflinePageItem& page, AddPageCallback callback) override;
  void MarkPageAccessed(int64_t offline_id) override;
  void DeletePagesWithCriteria(const PageCriteria& criteria,
                               DeletePageCallback callback) override;
  void DeleteCachedPagesByURLPredicate(const UrlPredicate& predicate,
                                       DeletePageCallback callback) override;

  void GetAllPages(MultipleOfflinePageItemCallback callback) override;
  void GetPageByOfflineId(int64_t offline_id,
                          SingleOfflinePageItemCallback callback) override;
  void GetPagesWithCriteria(const PageCriteria& criteria,
                            MultipleOfflinePageItemCallback callback) override;
  void GetOfflineIdsForClientId(const ClientId& client_id,
                                MultipleOfflineIdCallback callback) override;
  void StoreThumbnail(int64_t offline_id, std::string thumbnail) override;
  void StoreFavicon(int64_t offline_id, std::string favicon) override;
  void GetVisualsByOfflineId(
      int64_t offline_id,
      base::OnceCallback<void(std::unique_ptr<OfflinePageVisuals>)> callback)
      override;
  void GetVisualsAvailability(
      int64_t offline_id,
      base::OnceCallback<void(VisualsAvailability)> callback) override;
  const base::FilePath& GetArchiveDirectory(
      const std::string& name_space) const override;
  bool IsArchiveInInternalDir(const base::FilePath& file_path) const override;
  OfflineEventLogger* GetLogger() override;
  void PublishInternalArchive(
      const OfflinePageItem& offline_page,
      PublishPageCallback publish_done_callback) override;

  // Methods for testing only:
  OfflinePageMetadataStore* GetStoreForTesting() { return store_.get(); }
  void SetSkipClearingOriginalUrlForTesting() {
    skip_clearing_original_url_for_testing_ = true;
  }
  void DoNotRunMaintenanceTasksForTesting() {
    skip_maintenance_tasks_for_testing_ = true;
  }

 private:
  friend class OfflinePageModelTaskifiedTest;

  // Callbacks for saving pages.
  void InformSavePageDone(SavePageCallback calback,
                          SavePageResult result,
                          const ClientId& client_id,
                          int64_t offline_id);
  void OnAddPageForSavePageDone(SavePageCallback callback,
                                const OfflinePageItem& page_attempted,
                                base::Time add_page_start_time,
                                AddPageResult add_page_result,
                                int64_t offline_id);
  void OnCreateArchiveDone(const SavePageParams& save_page_params,
                           int64_t offline_id,
                           base::Time start_time,
                           std::unique_ptr<OfflinePageArchiver> archiver,
                           SavePageCallback callback,
                           OfflinePageArchiver::ArchiverResult archiver_result,
                           const GURL& saved_url,
                           const base::FilePath& file_path,
                           const std::u16string& title,
                           int64_t file_size,
                           const std::string& file_hash);

  // Callback for adding pages.
  void OnAddPageDone(const OfflinePageItem& page,
                     AddPageCallback callback,
                     AddPageResult result);

  // Callbacks for deleting pages.
  void OnDeleteDone(DeletePageCallback callback,
                    DeletePageResult result,
                    const std::vector<OfflinePageItem>& deleted_items);

  void OnStoreThumbnailDone(int64_t offline_id,
                            bool success,
                            std::string thumbnail);
  void OnStoreFaviconDone(int64_t offline_id,
                          bool success,
                          std::string favicon);

  // Methods for clearing temporary pages and performing consistency checks. The
  // latter are executed only once per Chrome session.
  void ScheduleMaintenanceTasks();
  void RunMaintenanceTasks(base::Time now, bool first_run);
  void OnPersistentPageConsistencyCheckDone(
      bool success,
      const std::vector<PublishedArchiveId>& ids_of_deleted_pages);

  // Callback for when PublishArchive has completd.
  void PublishArchiveDone(SavePageCallback save_page_callback,
                          base::Time publish_start_time,
                          const OfflinePageItem& offline_page,
                          PublishArchiveResult publish_results);

  // Callback for when publishing an internal archive has completed.
  void PublishInternalArchiveDone(PublishPageCallback publish_done_callback,
                                  const OfflinePageItem& offline_page,
                                  PublishArchiveResult publish_results);

  // Method for unpublishing the page from downloads.
  static void Unpublish(base::WeakPtr<OfflinePageArchivePublisher> publisher,
                        const std::vector<PublishedArchiveId>& publish_ids);

  // Other utility methods.
  void RemovePagesMatchingUrlAndNamespace(const OfflinePageItem& page);
  void CreateArchivesDirectoryIfNeeded();

  // Persistent store for offline page metadata.
  std::unique_ptr<OfflinePageMetadataStore> store_;

  // Manager for the offline archive files and directory.
  std::unique_ptr<ArchiveManager> archive_manager_;

  // Used for moving archives into public storage.
  std::unique_ptr<OfflinePageArchivePublisher> archive_publisher_;

  // The observers.
  base::ObserverList<Observer>::Unchecked observers_;

  // Logger to facilitate recording of events.
  OfflinePageModelEventLogger offline_event_logger_;

  // The task queue used for executing various tasks.
  TaskQueue task_queue_;

  // The last scheduling timestamp of the model maintenance tasks that took
  // place during the current Chrome session.
  base::Time last_maintenance_tasks_schedule_time_;

  // For testing only.
  // This value will be affecting the CreateArchiveTasks that are created by the
  // model to skip saving original_urls.
  bool skip_clearing_original_url_for_testing_;

  // For testing only.
  // This flag controls the execution of maintenance tasks; when false they will
  // not be executed.
  bool skip_maintenance_tasks_for_testing_;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<OfflinePageModelTaskified> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_MODEL_TASKIFIED_H_
