// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_DOWNLOAD_MANAGER_DELEGATE_H_
#define CHROMECAST_BROWSER_CAST_DOWNLOAD_MANAGER_DELEGATE_H_

#include "base/supports_user_data.h"
#include "content/public/browser/download_manager_delegate.h"

namespace chromecast {
namespace shell {

class CastDownloadManagerDelegate : public content::DownloadManagerDelegate,
                                    public base::SupportsUserData::Data {
 public:
  CastDownloadManagerDelegate();

  CastDownloadManagerDelegate(const CastDownloadManagerDelegate&) = delete;
  CastDownloadManagerDelegate& operator=(const CastDownloadManagerDelegate&) =
      delete;

  ~CastDownloadManagerDelegate() override;

  // content::DownloadManagerDelegate implementation:
  void GetNextId(content::DownloadIdCallback callback) override;
  bool DetermineDownloadTarget(
      download::DownloadItem* item,
      download::DownloadTargetCallback* callback) override;
  bool ShouldCompleteDownload(download::DownloadItem* item,
                              base::OnceClosure complete_callback) override;
  bool ShouldOpenDownload(
      download::DownloadItem* item,
      content::DownloadOpenDelayedCallback callback) override;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_DOWNLOAD_MANAGER_DELEGATE_H_
