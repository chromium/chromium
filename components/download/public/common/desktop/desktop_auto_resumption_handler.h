// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DESKTOP_DESKTOP_AUTO_RESUMPTION_HANDLER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DESKTOP_DESKTOP_AUTO_RESUMPTION_HANDLER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_item.h"

namespace download {

class COMPONENTS_DOWNLOAD_EXPORT DesktopAutoResumptionHandler
    : public DownloadItem::Observer {
 public:
  static DesktopAutoResumptionHandler* Get();

  DesktopAutoResumptionHandler(const DesktopAutoResumptionHandler&) = delete;
  DesktopAutoResumptionHandler& operator=(const DesktopAutoResumptionHandler&) =
      delete;
  DesktopAutoResumptionHandler();
  ~DesktopAutoResumptionHandler() override;
  void OnDownloadUpdated(DownloadItem* item) override;
  void OnDownloadDestroyed(DownloadItem* item) override;
  bool IsAutoResumableDownload(DownloadItem* item) const;

 private:
  base::TimeDelta ComputeBackoffDelay(int retry_count);
  void MaybeResumeDownload(std::string guid);
  std::map<std::string, DownloadItem*> resumable_downloads_;

  base::WeakPtrFactory<DesktopAutoResumptionHandler> weak_factory_{this};
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DESKTOP_DESKTOP_AUTO_RESUMPTION_HANDLER_H_
