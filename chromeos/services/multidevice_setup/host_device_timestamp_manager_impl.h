// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_DEVICE_TIMESTAMP_MANAGER_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_DEVICE_TIMESTAMP_MANAGER_IMPL_H_

#include <memory>

#include "base/optional.h"
#include "base/time/time.h"
#include "chromeos/services/multidevice_setup/host_device_timestamp_manager.h"
#include "chromeos/services/multidevice_setup/host_status_provider.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
}

namespace chromeos {

namespace multidevice_setup {

// Concrete HostDeviceTimestampManager implementation.
class HostDeviceTimestampManagerImpl : public HostDeviceTimestampManager,
                                       public HostStatusProvider::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<HostDeviceTimestampManager> BuildInstance(
        HostStatusProvider* host_status_provider,
        PrefService* pref_service,
        base::Clock* clock);

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~HostDeviceTimestampManagerImpl() override;

  // HostDeviceTimestampManager:
  bool WasHostSetFromThisChromebook() override;
  base::Optional<base::Time> GetLatestSetupFlowCompletionTimestamp() override;
  base::Optional<base::Time> GetLatestVerificationTimestamp() override;

 private:
  static const char kWasHostSetFromThisChromebookPrefName[];
  static const char kSetupFlowCompletedPrefName[];
  static const char kHostVerifiedUpdateReceivedPrefName[];

  HostDeviceTimestampManagerImpl(HostStatusProvider* host_status_provider,
                                 PrefService* pref_service,
                                 base::Clock* clock);

  // HostStatusProvider::Observer:
  void OnHostStatusChange(const HostStatusProvider::HostStatusWithDevice&
                              host_status_with_device) override;

  HostStatusProvider* host_status_provider_;
  PrefService* pref_service_;
  base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(HostDeviceTimestampManagerImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_DEVICE_TIMESTAMP_MANAGER_IMPL_H_
