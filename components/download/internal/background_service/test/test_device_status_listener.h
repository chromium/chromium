// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_TEST_DEVICE_STATUS_LISTENER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_TEST_DEVICE_STATUS_LISTENER_H_

#include "base/memory/weak_ptr.h"
#include "components/download/internal/background_service/scheduler/device_status_listener.h"

namespace download {
namespace test {

// Test device status listener can directly notify the observer about battery
// and network changes, without calling external class methods.
class TestDeviceStatusListener : public DeviceStatusListener {
 public:
  TestDeviceStatusListener();
  ~TestDeviceStatusListener() override;

  // Notifies observer with current device status.
  void NotifyObserver(const DeviceStatus& device_status);

  // Sets the device status without notifying the observer.
  // If called before Start() is called, may notify the observer.
  void SetDeviceStatus(const DeviceStatus& status);

  // DeviceStatusListener implementation.
  void Start(const base::TimeDelta& start_delay) override;
  void Stop() override;

 private:
  void StartAfterDelay();

  base::WeakPtrFactory<TestDeviceStatusListener> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestDeviceStatusListener);
};

}  // namespace test
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_TEST_DEVICE_STATUS_LISTENER_H_
