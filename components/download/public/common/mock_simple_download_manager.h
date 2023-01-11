// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_MOCK_SIMPLE_DOWNLOAD_MANAGER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_MOCK_SIMPLE_DOWNLOAD_MANAGER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "components/download/public/common/simple_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace download {

class MockSimpleDownloadManager : public SimpleDownloadManager {
 public:
  MockSimpleDownloadManager();
  ~MockSimpleDownloadManager() override;

  // Dispatches an OnNewDownloadCreated() notification to observers.
  void NotifyOnNewDownloadCreated(DownloadItem* item);

  // Notifies observers that downloads is initialized.
  void NotifyOnDownloadInitialized();

  // Get the DownloadUrlParameters passed to |DownloadUrl|.
  const DownloadUrlParameters* GetDownloadUrlParameters();

  // SimpleDownloadManager implementation.
  void DownloadUrl(std::unique_ptr<DownloadUrlParameters> params) override;

  MOCK_METHOD1(CanDownload, bool(DownloadUrlParameters*));
  MOCK_METHOD1(DownloadUrlMock, void(DownloadUrlParameters*));
  MOCK_METHOD1(GetDownloadByGuid, DownloadItem*(const std::string&));
  MOCK_METHOD1(GetAllDownloads, void(DownloadVector* downloads));

 private:
  base::ObserverList<Observer>::Unchecked observers_;
  std::unique_ptr<DownloadUrlParameters> params_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_MOCK_SIMPLE_DOWNLOAD_MANAGER_H_
