// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_SIMPLE_DOWNLOAD_MANAGER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_SIMPLE_DOWNLOAD_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_url_parameters.h"

namespace download {

class DownloadItem;

// Download manager that provides simple functionalities for callers to carry
// out a download task.
class COMPONENTS_DOWNLOAD_EXPORT SimpleDownloadManager {
 public:
  class Observer {
   public:
    Observer() = default;

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    virtual ~Observer() = default;

    virtual void OnDownloadsInitialized() {}
    virtual void OnManagerGoingDown() {}
    virtual void OnDownloadCreated(DownloadItem* item) {}
  };

  SimpleDownloadManager();
  virtual ~SimpleDownloadManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Download a URL given by the |params|. Returns true if the download could
  // take place, or false otherwise.
  virtual void DownloadUrl(
      std::unique_ptr<DownloadUrlParameters> parameters) = 0;

  // Returns whether the manager can handle this download.
  virtual bool CanDownload(DownloadUrlParameters* parameters) = 0;

  using DownloadVector = std::vector<raw_ptr<DownloadItem, VectorExperimental>>;
  // Add all initialized download items to |downloads|, no matter the type or
  // state, without clearing |downloads| first. If active downloads are not
  // initialized, this call will not return them. Caller should call
  // GetUninitializedActiveDownloadsIfAny() below to retrieve uninitialized
  // active downloads.
  virtual void GetAllDownloads(DownloadVector* downloads) = 0;

  // Gets all the active downloads that are initialized yet.
  virtual void GetUninitializedActiveDownloadsIfAny(DownloadVector* downloads) {
  }

  // Get the download item for |guid|.
  virtual DownloadItem* GetDownloadByGuid(const std::string& guid) = 0;

  // Checks whether downloaded files still exist. Updates state of downloads
  // that refer to removed files. The check runs in the background and may
  // finish asynchronously after this method returns.
  virtual void CheckForHistoryFilesRemoval() {}

 protected:
  // Called when the manager is initailized.
  void OnInitialized();

  // Called when a new download is created.
  void OnNewDownloadCreated(DownloadItem* download);

  // Notify observers that this object is initialized.
  void NotifyInitialized();

  // Whether this object is initialized.
  bool initialized_ = false;

  // Observers that want to be notified of changes to the set of downloads.
  base::ObserverList<Observer>::Unchecked simple_download_manager_observers_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_SIMPLE_DOWNLOAD_MANAGER_H_
