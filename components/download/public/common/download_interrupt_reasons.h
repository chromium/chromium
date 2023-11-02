// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_INTERRUPT_REASONS_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_INTERRUPT_REASONS_H_

#include <string>

#include "components/download/public/common/download_export.h"

namespace download {

enum DownloadInterruptReason {
  DOWNLOAD_INTERRUPT_REASON_NONE = 0,

#define INTERRUPT_REASON(name, value) DOWNLOAD_INTERRUPT_REASON_##name = value,

#include "components/download/public/common/download_interrupt_reason_values.h"

#undef INTERRUPT_REASON
};

std::string COMPONENTS_DOWNLOAD_EXPORT
DownloadInterruptReasonToString(DownloadInterruptReason error);

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_INTERRUPT_REASONS_H_
