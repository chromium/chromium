// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_MODEL_TASKIFIED_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_MODEL_TASKIFIED_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/core/model/clear_storage_task.h"
#include "components/offline_pages/core/offline_page_archiver.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_model_event_logger.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "components/offline_pages/task/task_queue.h"

class GURL;
namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace offline_pages {

struct ClientId;
struct OfflinePageItem;

class ArchiveManager;
class ClientPolicyController;
class OfflinePageArchiver;
class OfflinePageMetadataStore;
class SystemDownloadManager;

// Implementaion of OfflinePageModel, which is a service for saving pages
// offline. It's an entry point to get information about Offline Pages and the
// base of related Offline Pages features.
// It owns a database which stores offline metadata, and uses TaskQueue for
// executing various tasks, including database operation or other process that
// needs to run on a background thread.
class OfflinePageModelTaskified : public OfflinePageModel,
                                  public TaskQueue::Delegate {
 public:
  // Initial delay after which a list of items for upgrade will be generated.
  static constexpr base::TimeDelta kInitialUpgradeSelectionDelay =
      base::TimeDelta::FromSeconds(45);

  // Delay between the scheduling and actual running of maintenance tasks. To
  // not cause the re-opening of the metadata store this delay should be kept
  // smaller than OfflinePageMetadataStore::kClosingDelay.
  static constexpr base::TimeDelta kMaintenanceTasksDelay =
      base::TimeDelta::FromSeconds(10);

  // Minimum delay between runs of maintenance tasks during a Chrome session.
  static constexpr base::TimeDelta kClearStorageInterval =
      base::TimeDelta::FromMinutes(30);

  OfflinePageModelTaskified(
      std::unique_ptr<OfflinePageMetadataStore> store,
      std::unique_ptr<ArchiveManager> archive_manager,
      std::unique_ptr<SystemDownloadManager> download_manager,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      base::Clock* clock);
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

  void DeletePagesByOfflineId(const std::vector<int64_t>& offline_ids,
                              DeletePageCallback callback) override;
  void DeletePagesByClientIds(const std::vector<ClientId>& client_ids,
                              DeletePageCallback callback) override;
  void DeletePagesByClientIdsAndOrigin(const std::vector<ClientId>& client_ids,
                                       const std::string& origin,
                                       DeletePageCallback callback) override;
  void DeleteCachedPagesByURLPredicate(const UrlPredicate& predicate,
                                       DeletePageCallback callback) override;

  void GetAllPages(MultipleOfflinePageItemCallback callback) override;
  void GetPageByOfflineId(int64_t offline_id,
                          SingleOfflinePageItemCallback callback) override;
  void GetPageByGuid(const std::string& guid,
                     SingleOfflinePageItemCallback callback) override;
  void GetPagesByClientIds(const std::vector<ClientId>& client_ids,
                           MultipleOfflinePageItemCallback callback) override;
  void GetPagesByURL(const GURL& url,
                     MultipleOfflinePageItemCallback callback) override;
  void GetPagesByNamespace(const std::string& name_space,
                           MultipleOfflinePageItemCallback callback) override;
  void GetPagesRemovedOnCacheReset(
      MultipleOfflinePageItemCallback callback) override;
  void GetPagesSupportedByDownloads(
      MultipleOfflinePageItemCallback callback) override;
  void GetPagesByRequestOrigin(
      const std::string& request_origin,
      MultipleOfflinePageItemCallback callback) override;
  void GetPageBySizeAndDigest(int64_t file_size,
                              const std::string& digest,
                              SingleOfflinePageItemCallback callback) override;
  void GetOfflineIdsForClientId(const ClientId& client_id,
                                MultipleOfflineIdCallback callback) override;
  void StoreThumbnail(const OfflinePageThumbnail& thumb) override;
  void GetThumbnailByOfflineId(
      int64_t offline_id,
      base::OnceCallback<void(std::unique_ptr<OfflinePageThumbnail>)> callback)
      override;
  void HasThumbnailForOfflineId(
      int64_t offline_id,
      base::OnceCallback<void(bool)> callback) override;
  const base::FilePath& GetInternalArchiveDirectory(
      const std::string& name_space) const override;
  bool IsArchiveInInternalDir(const base::FilePath& file_path) const override;
  ClientPolicyController* GetPolicyController() override;
  OfflineEventLogger* GetLogger() override;
  void PublishInternalArchive(
      const OfflinePageItem& offline_page,
      std::unique_ptr<OfflinePageArchiver> archiver,
      PublishPageCallback publish_done_callback) override;

  // Methods for testing only:
  OfflinePageMetadataStore* GetStoreForTesting() { return store_.get(); }
  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }
  void SetSkipClearingOriginalUrlForTesting() {
    skip_clearing_original_url_for_testing_ = true;
  }
  void DoNotRunMaintenanceTasksForTesting() {
    skip_maintenance_tasks_for_testing_ = true;
  }

 private:
  // TODO(romax): https://crbug.com/791115, remove the friend class usage.
  friend class OfflinePageModelTaskifiedTest;

  // Callbacks for saving pages.
  void InformSavePageDone(SavePageCallback calback,
                          SavePageResult result,
                          const ClientId& client_id,
                          int64_t offline_id);
  void OnAddPageForSavePageDone(SavePageCallback callback,
                                const OfflinePageItem& page_attempted,
                                AddPageResult add_page_result,
                                int64_t offline_id);
  void OnCreateArchiveDone(const SavePageParams& save_page_params,
                           int64_t offline_id,
                           const base::Time& start_time,
                           std::unique_ptr<OfflinePageArchiver> archiver,
                           SavePageCallback callback,
                           OfflinePageArchiver::ArchiverResult archiver_result,
                           const GURL& saved_url,
                           const base::FilePath& file_path,
                           const base::string16& title,
                           int64_t file_size,
                           const std::string& file_hash);

  // Callback for adding pages.
  void OnAddPageDone(const OfflinePageItem& page,
                     AddPageCallback callback,
                     AddPageResult result);

  // Callbacks for deleting pages.
  void OnDeleteDone(
      DeletePageCallback callback,
      DeletePageResult result,
      const std::vector<OfflinePageModel::DeletedPageInfo>& infos);

  void OnStoreThumbnailDone(const OfflinePageThumbnail& thumbnail,
                            bool success);

  // Methods for clearing temporary pages and performing consistency checks. The
  // latter are executed only once per Chrome session.
  void ScheduleMaintenanceTasks();
  void RunMaintenanceTasks(const base::Time now, bool first_run);
  void OnClearCachedPagesDone(size_t deleted_page_count,
                              ClearStorageTask::ClearStorageResult result);
  void OnPersistentPageConsistencyCheckDone(
      bool success,
      const std::vector<int64_t>& pages_deleted);

  // Method for upgrade to public storage.
  void PostSelectItemsMarkedForUpgrade();
  void SelectItemsMarkedForUpgrade();
  void OnSelectItemsMarkedForUpgradeDone(
      const MultipleOfflinePageItemResult& pages_for_upgrade);

  // Callback for when PublishArchive has completd.
  void PublishArchiveDone(std::unique_ptr<OfflinePageArchiver> archiver,
                          SavePageCallback save_page_callback,
                          const OfflinePageItem& offline_page,
                          PublishArchiveResult publish_results);

  // Callback for when publishing an internal archive has completed.
  void PublishInternalArchiveDone(std::unique_ptr<OfflinePageArchiver> archiver,
                                  PublishPageCallback publish_done_callback,
                                  const OfflinePageItem& offline_page,
                                  PublishArchiveResult publish_results);

  // Method for unpublishing the page from the system download manager.
  static void RemoveFromDownloadManager(
      SystemDownloadManager* download_manager,
      const std::vector<int64_t>& system_download_ids);

  // Other utility methods.
  void RemovePagesMatchingUrlAndNamespace(const OfflinePageItem& page);
  void CreateArchivesDirectoryIfNeeded();
  base::Time GetCurrentTime();

  // Persistent store for offline page metadata.
  std::unique_ptr<OfflinePageMetadataStore> store_;

  // Manager for the offline archive files and directory.
  std::unique_ptr<ArchiveManager> archive_manager_;

  // Manages interaction with the OS download manager, if present.
  std::unique_ptr<SystemDownloadManager> download_manager_;

  // Controller of the client policies.
  std::unique_ptr<ClientPolicyController> policy_controller_;

  // The observers.
  base::ObserverList<Observer>::Unchecked observers_;

  // Clock for testing only.
  base::Clock* clock_ = nullptr;

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

  base::WeakPtrFactory<OfflinePageModelTaskified> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageModelTaskified);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_MODEL_TASKIFIED_H_
