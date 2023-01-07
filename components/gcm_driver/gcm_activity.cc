// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_activity.h"

namespace gcm {

Activity::Activity()
    : time(base::Time::Now()) {
}

Activity::~Activity() {
}

CheckinActivity::CheckinActivity() {
}

CheckinActivity::~CheckinActivity() {
}

ConnectionActivity::ConnectionActivity() {
}

ConnectionActivity::~ConnectionActivity() {
}

RegistrationActivity::RegistrationActivity() {
}

RegistrationActivity::~RegistrationActivity() {
}

ReceivingActivity::ReceivingActivity()
    : message_byte_size(0) {
}

ReceivingActivity::~ReceivingActivity() {
}

SendingActivity::SendingActivity() {
}

SendingActivity::~SendingActivity() {
}

DecryptionFailureActivity::DecryptionFailureActivity() {
}

DecryptionFailureActivity::~DecryptionFailureActivity() {
}

RecordedActivities::RecordedActivities() {
}

RecordedActivities::RecordedActivities(const RecordedActivities& other) =
    default;

RecordedActivities::~RecordedActivities() {
}

}  // namespace gcm
