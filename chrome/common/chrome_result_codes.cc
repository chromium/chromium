// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_result_codes.h"

namespace chrome {

bool IsNormalResultCode(ResultCode code) {
  // These result codes are normal exit, but are needed to signal to content
  // that the process should terminate early. These result codes should be
  // translated back to the normal exit code to indicate nothing went wrong
  // here.
  if (code == RESULT_CODE_NORMAL_EXIT_UPGRADE_RELAUNCHED ||
      code == RESULT_CODE_NORMAL_EXIT_PACK_EXTENSION_SUCCESS ||
      code == RESULT_CODE_NORMAL_EXIT_PROCESS_NOTIFIED) {
    return true;
  }

  return false;
}

}  // namespace chrome
