// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_MOCK_ON_DEVICE_ENCRYPTION_STATE_TRACKER_OBSERVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_MOCK_ON_DEVICE_ENCRYPTION_STATE_TRACKER_OBSERVER_H_

#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockOnDeviceEncryptionStateTrackerObserver
    : public OnDeviceEncryptionStateTracker::Observer {
 public:
  MockOnDeviceEncryptionStateTrackerObserver();
  ~MockOnDeviceEncryptionStateTrackerObserver() override;

  MOCK_METHOD(void,
              OnDeviceEncryptionStateChanged,
              (OnDeviceEncryptionStateTracker * tracker,
               OnDeviceEncryptionState previous_state,
               OnDeviceEncryptionState new_state),
              (override));
  MOCK_METHOD(void,
              OnDeviceEncryptionStateTrackerShuttingDown,
              (OnDeviceEncryptionStateTracker * tracker),
              (override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_MOCK_ON_DEVICE_ENCRYPTION_STATE_TRACKER_OBSERVER_H_
