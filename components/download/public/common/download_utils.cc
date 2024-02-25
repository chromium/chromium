// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_utils.h"

namespace download {

bool IsInterruptedDownloadAutoResumable(download::DownloadItem* download_item,
                                        int auto_resumption_size_limit) {
  DCHECK_EQ(download::DownloadItem::INTERRUPTED, download_item->GetState());
  if (download_item->IsDangerous()) {
    return false;
  }

  if (!download_item->GetURL().SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  if (download_item->GetBytesWasted() > auto_resumption_size_limit) {
    return false;
  }

  if (download_item->GetTargetFilePath().empty()) {
    return false;
  }

  // TODO(shaktisahu): Use DownloadItemImpl::kMaxAutoResumeAttempts.
  if (download_item->GetAutoResumeCount() >= 5) {
    return false;
  }

  int interrupt_reason = download_item->GetLastReason();
  DCHECK_NE(interrupt_reason, download::DOWNLOAD_INTERRUPT_REASON_NONE);
  return interrupt_reason ==
             download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT ||
         interrupt_reason ==
             download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED ||
         interrupt_reason ==
             download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED ||
         interrupt_reason == download::DOWNLOAD_INTERRUPT_REASON_CRASH;
}

}  // namespace download
