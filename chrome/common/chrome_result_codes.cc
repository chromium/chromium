// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_result_codes.h"

bool IsNormalResultCode(ResultCode code) {
  // These result codes are normal exit, but are needed to signal to content
  // that the process should terminate early. These result codes should be
  // translated back to the normal exit code to indicate nothing went wrong
  // here.
  if (code == CHROME_RESULT_CODE_NORMAL_EXIT_UPGRADE_RELAUNCHED ||
      code == CHROME_RESULT_CODE_NORMAL_EXIT_PACK_EXTENSION_SUCCESS ||
      code == CHROME_RESULT_CODE_NORMAL_EXIT_PROCESS_NOTIFIED ||
      code == CHROME_RESULT_CODE_NORMAL_EXIT_AUTO_DE_ELEVATED) {
    return true;
  }

  return false;
}

