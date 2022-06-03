// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UNITTEST_UTIL_H_
#define CHROME_UPDATER_UNITTEST_UTIL_H_

#include "chrome/updater/update_service.h"

namespace updater {

extern const char kChromeAppId[];

bool operator==(const UpdateService::UpdateState& lhs,
                const UpdateService::UpdateState& rhs);
inline bool operator!=(const UpdateService::UpdateState& lhs,
                       const UpdateService::UpdateState& rhs) {
  return !(lhs == rhs);
}

}  // namespace updater

#endif  // CHROME_UPDATER_UNITTEST_UTIL_H_
