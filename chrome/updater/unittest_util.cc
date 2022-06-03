// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/unittest_util.h"

#include "base/version.h"

namespace updater {

const char kChromeAppId[] = "{8A69D345-D564-463C-AFF1-A69D9E530F96}";

bool operator==(const UpdateService::UpdateState& lhs,
                const UpdateService::UpdateState& rhs) {
  const bool versions_equal =
      (lhs.next_version.IsValid() && rhs.next_version.IsValid() &&
       lhs.next_version == rhs.next_version) ||
      (!lhs.next_version.IsValid() && !rhs.next_version.IsValid());
  return versions_equal && lhs.app_id == rhs.app_id && lhs.state == rhs.state &&
         lhs.downloaded_bytes == rhs.downloaded_bytes &&
         lhs.total_bytes == rhs.total_bytes &&
         lhs.install_progress == rhs.install_progress &&
         lhs.error_category == rhs.error_category &&
         lhs.error_code == rhs.error_code && lhs.extra_code1 == rhs.extra_code1;
}


}  // namespace updater
