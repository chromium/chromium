// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_interrupt_reasons.h"

namespace download {

std::string DownloadInterruptReasonToString(DownloadInterruptReason error) {
#define INTERRUPT_REASON(name, value)    \
  case DOWNLOAD_INTERRUPT_REASON_##name: \
    return #name;

  switch (error) {
    INTERRUPT_REASON(NONE, 0)

#include "components/download/public/common/download_interrupt_reason_values.h"

    default:
      break;
  }

#undef INTERRUPT_REASON

  return "Unknown error";
}

}  // namespace download
