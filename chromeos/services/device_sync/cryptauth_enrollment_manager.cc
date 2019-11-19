// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_enrollment_manager.h"

namespace chromeos {

namespace device_sync {

CryptAuthEnrollmentManager::CryptAuthEnrollmentManager() = default;

CryptAuthEnrollmentManager::~CryptAuthEnrollmentManager() = default;

void CryptAuthEnrollmentManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CryptAuthEnrollmentManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CryptAuthEnrollmentManager::NotifyEnrollmentStarted() {
  for (auto& observer : observers_)
    observer.OnEnrollmentStarted();
}

void CryptAuthEnrollmentManager::NotifyEnrollmentFinished(bool success) {
  for (auto& observer : observers_)
    observer.OnEnrollmentFinished(success);
}

}  // namespace device_sync

}  // namespace chromeos
