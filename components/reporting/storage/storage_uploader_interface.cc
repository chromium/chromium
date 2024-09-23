// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/reporting/storage/storage_uploader_interface.h"

namespace reporting {

UploaderInterface::UploaderInterface() = default;
UploaderInterface::~UploaderInterface() = default;

// static
std::string_view UploaderInterface::ReasonToString(UploadReason reason) {
  static const char*
      reason_to_string[static_cast<uint32_t>(UploadReason::MAX_REASON)] = {
          "UNKNOWN",         "MANUAL",        "KEY_DELIVERY",     "PERIODIC",
          "IMMEDIATE_FLUSH", "FAILURE_RETRY", "INCOMPLETE_RETRY", "INIT_RESUME",
      };

  return reason < UploadReason::MAX_REASON
             ? reason_to_string[static_cast<uint32_t>(reason)]
             : "ILLEGAL";
}

}  // namespace reporting
