// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_START_OBSERVER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_START_OBSERVER_H_

#include "components/download/public/common/download_export.h"

namespace download {

class DownloadItem;

// Class for listening to download start event.
class COMPONENTS_DOWNLOAD_EXPORT DownloadStartObserver {
 public:
  // Called when a download is started, either from a new download or
  // a resumed download. Must be called on the UI thread.
  virtual void OnDownloadStarted(DownloadItem* download_item) = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_START_OBSERVER_H_
