// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_result_codes.h"

namespace chrome {

bool IsNormalResultCode(ResultCode code) {
  // This result code is a normal exit, but is needed to signal to content that
  // the process should terminate early. This result code should be translated
  // back to the normal exit code to indicate nothing went wrong here.
  if (code == RESULT_CODE_NORMAL_EXIT_UPGRADE_RELAUNCHED)
    return true;

  return false;
}

}  // namespace chrome
