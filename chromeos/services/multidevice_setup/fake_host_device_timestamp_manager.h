// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_DEVICE_TIMESTAMP_MANAGER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_DEVICE_TIMESTAMP_MANAGER_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "chromeos/services/multidevice_setup/host_device_timestamp_manager.h"

namespace chromeos {

namespace multidevice_setup {

class FakeHostDeviceTimestampManager : public HostDeviceTimestampManager {
 public:
  FakeHostDeviceTimestampManager();
  ~FakeHostDeviceTimestampManager() override;

  void set_was_host_set_from_this_chromebook(
      bool was_host_set_from_this_chromebook);
  void set_completion_timestamp(const base::Time& timestamp);
  void set_verification_timestamp(const base::Time& timestamp);

 private:
  // HostDeviceTimestampManager:
  bool WasHostSetFromThisChromebook() override;
  base::Optional<base::Time> GetLatestSetupFlowCompletionTimestamp() override;
  base::Optional<base::Time> GetLatestVerificationTimestamp() override;

  bool was_host_set_from_this_chromebook_;
  base::Optional<base::Time> completion_time_;
  base::Optional<base::Time> verification_time_;
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_DEVICE_TIMESTAMP_MANAGER_H_
