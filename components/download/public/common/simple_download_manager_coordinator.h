// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_SIMPLE_DOWNLOAD_MANAGER_COORDINATOR_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_SIMPLE_DOWNLOAD_MANAGER_COORDINATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/simple_download_manager.h"
#include "components/keyed_service/core/keyed_service.h"

namespace download {

class AllDownloadEventNotifier;
class DownloadItem;

// This object allows swapping between different SimppleDownloadManager
// instances so that callers don't need to know about the swap. It can
// be created before full browser process is launched, so that download
// can be handled without full browser.
class COMPONENTS_DOWNLOAD_EXPORT SimpleDownloadManagerCoordinator
    : public KeyedService,
      public SimpleDownloadManager::Observer {
 public:
  class Observer {
   public:
    Observer() = default;
    virtual ~Observer() = default;

    virtual void OnManagerGoingDown(
        SimpleDownloadManagerCoordinator* coordinator) {}
    virtual void OnDownloadsInitialized(bool active_downloads_only) {}
    virtual void OnDownloadCreated(DownloadItem* item) {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  using DownloadWhenFullManagerStartsCallBack =
      base::RepeatingCallback<void(std::unique_ptr<DownloadUrlParameters>)>;
  SimpleDownloadManagerCoordinator(const DownloadWhenFullManagerStartsCallBack&
                                       download_when_full_manager_starts_cb,
                                   bool record_full_download_manager_delay);
  ~SimpleDownloadManagerCoordinator() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetSimpleDownloadManager(SimpleDownloadManager* simple_download_manager,
                                bool has_all_history_downloads);

  // See download::DownloadUrlParameters for details about controlling the
  // download.
  void DownloadUrl(std::unique_ptr<DownloadUrlParameters> parameters);

  // Gets all the downloads. Caller needs to call has_all_history_downloads() to
  // check if all downloads are initialized. If only active downloads are
  // initialized, this method will only return all active downloads.
  void GetAllDownloads(std::vector<DownloadItem*>* downloads);

  // Get the download item for |guid|.
  DownloadItem* GetDownloadByGuid(const std::string& guid);

  // Returns a non-empty notifier to be used for observing download events.
  AllDownloadEventNotifier* GetNotifier();

  // Whether this object is initialized.
  bool initialized() const { return initialized_; }

  bool has_all_history_downloads() const { return has_all_history_downloads_; }

  // Checks whether downloaded files still exist. Updates state of downloads
  // that refer to removed files. The check runs in the background and may
  // finish asynchronously after this method returns.
  void CheckForExternallyRemovedDownloads();

 private:
  // SimpleDownloadManager::Observer implementation.
  void OnDownloadsInitialized() override;
  void OnManagerGoingDown() override;
  void OnDownloadCreated(DownloadItem* item) override;

  SimpleDownloadManager* simple_download_manager_;

  // Object for notifying others about various download events.
  std::unique_ptr<AllDownloadEventNotifier> notifier_;

  // Whether all the history downloads are ready.
  bool has_all_history_downloads_;

  // Whether current SimpleDownloadManager has all history downloads.
  bool current_manager_has_all_history_downloads_;

  // Whether this object is initialized and active downloads are ready to be
  // retrieved.
  bool initialized_;

  // Callback to download the url when full manager becomes ready.
  DownloadWhenFullManagerStartsCallBack download_when_full_manager_starts_cb_;

  // Observers that want to be notified of changes to the set of downloads.
  base::ObserverList<Observer>::Unchecked observers_;

  // Time when this object was created.
  base::TimeTicks creation_time_ticks_;

  base::WeakPtrFactory<SimpleDownloadManagerCoordinator> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SimpleDownloadManagerCoordinator);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_SIMPLE_DOWNLOAD_MANAGER_COORDINATOR_H_
