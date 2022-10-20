// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_android_sms_pairing_state_tracker.h"

#include "url/gurl.h"

namespace ash {

namespace multidevice_setup {

FakeAndroidSmsPairingStateTracker::FakeAndroidSmsPairingStateTracker() {}

FakeAndroidSmsPairingStateTracker::~FakeAndroidSmsPairingStateTracker() =
    default;

void FakeAndroidSmsPairingStateTracker::SetPairingComplete(
    bool is_pairing_complete) {
  if (is_pairing_complete == is_pairing_complete_)
    return;

  is_pairing_complete_ = is_pairing_complete;
  NotifyPairingStateChanged();
}

bool FakeAndroidSmsPairingStateTracker::IsAndroidSmsPairingComplete() {
  return is_pairing_complete_;
}

}  // namespace multidevice_setup

}  // namespace ash
