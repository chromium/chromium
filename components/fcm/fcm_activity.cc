// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fcm/fcm_activity.h"

namespace fcm {

Activity::Activity() : time(base::Time::Now()) {}

Activity::~Activity() = default;

CheckinActivity::CheckinActivity() = default;

CheckinActivity::~CheckinActivity() = default;

ConnectionActivity::ConnectionActivity() = default;

ConnectionActivity::~ConnectionActivity() = default;

RegistrationActivity::RegistrationActivity() = default;

RegistrationActivity::~RegistrationActivity() = default;

ReceivingActivity::ReceivingActivity() : message_byte_size(0) {}

ReceivingActivity::~ReceivingActivity() = default;

SendingActivity::SendingActivity() = default;

SendingActivity::~SendingActivity() = default;

DecryptionFailureActivity::DecryptionFailureActivity() = default;

DecryptionFailureActivity::~DecryptionFailureActivity() = default;

RecordedActivities::RecordedActivities() = default;

RecordedActivities::RecordedActivities(const RecordedActivities& other) =
    default;

RecordedActivities::~RecordedActivities() = default;

}  // namespace fcm
