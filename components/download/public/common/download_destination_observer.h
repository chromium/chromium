// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_DESTINATION_OBSERVER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_DESTINATION_OBSERVER_H_

#include <stdint.h>

#include <memory>

#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "crypto/secure_hash.h"

namespace download {

// Class that receives asynchronous events from a DownloadDestination about
// downloading progress and completion.  These should report status when the
// data arrives at its final location; i.e. DestinationUpdate should be
// called after the destination is finished with whatever operation it
// is doing on the data described by |bytes_so_far| and DestinationCompleted
// should only be called once that is true for all data.
//
// All methods are invoked on the same thread the observer was created.
//
// Note that this interface does not deal with cross-thread lifetime issues.
class COMPONENTS_DOWNLOAD_EXPORT DownloadDestinationObserver {
 public:
  virtual ~DownloadDestinationObserver();

  virtual void DestinationUpdate(
      int64_t bytes_so_far,
      int64_t bytes_per_sec,
      const std::vector<DownloadItem::ReceivedSlice>& received_slices) = 0;

  virtual void DestinationError(
      DownloadInterruptReason reason,
      int64_t bytes_so_far,
      std::unique_ptr<crypto::SecureHash> hash_state) = 0;

  virtual void DestinationCompleted(
      int64_t total_bytes,
      std::unique_ptr<crypto::SecureHash> hash_state) = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_DESTINATION_OBSERVER_H_
