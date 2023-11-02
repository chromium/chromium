// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_INTERRUPT_REASONS_UTILS_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_INTERRUPT_REASONS_UTILS_H_

#include "base/files/file.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_stream.mojom.h"
#include "net/base/net_errors.h"

namespace download {

enum DownloadInterruptSource {
  DOWNLOAD_INTERRUPT_FROM_DISK,
  DOWNLOAD_INTERRUPT_FROM_NETWORK,
  DOWNLOAD_INTERRUPT_FROM_SERVER,
  DOWNLOAD_INTERRUPT_FROM_USER,
  DOWNLOAD_INTERRUPT_FROM_CRASH
};

// Safe to call from any thread.
DownloadInterruptReason COMPONENTS_DOWNLOAD_EXPORT
ConvertNetErrorToInterruptReason(net::Error file_error,
                                 DownloadInterruptSource source);

// Safe to call from any thread.
DownloadInterruptReason COMPONENTS_DOWNLOAD_EXPORT
ConvertFileErrorToInterruptReason(base::File::Error file_error);

// Safe to call from any thread.
DownloadInterruptReason COMPONENTS_DOWNLOAD_EXPORT
ConvertMojoNetworkRequestStatusToInterruptReason(
    mojom::NetworkRequestStatus status);

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_INTERRUPT_REASONS_UTILS_H_
