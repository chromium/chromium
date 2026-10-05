// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fcm/engine/checkin_info.h"

namespace fcm {

CheckinInfo::CheckinInfo() = default;

CheckinInfo::CheckinInfo(uint64_t android_id, uint64_t secret)
    : android_id_(android_id), secret_(secret) {}

CheckinInfo::~CheckinInfo() = default;

CheckinInfo::CheckinInfo(const CheckinInfo&) = default;
CheckinInfo& CheckinInfo::operator=(const CheckinInfo&) = default;

CheckinInfo::CheckinInfo(CheckinInfo&&) = default;
CheckinInfo& CheckinInfo::operator=(CheckinInfo&&) = default;

void CheckinInfo::Reset() {
  android_id_ = 0;
  secret_ = 0;
  accounts_set_ = false;
}

}  // namespace fcm
