// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_DEVICE_TIMESTAMP_MANAGER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_DEVICE_TIMESTAMP_MANAGER_IMPL_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/services/multidevice_setup/host_device_timestamp_manager.h"
#include "chromeos/ash/services/multidevice_setup/host_status_provider.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
}

namespace ash {

namespace multidevice_setup {

// Concrete HostDeviceTimestampManager implementation.
class HostDeviceTimestampManagerImpl : public HostDeviceTimestampManager,
                                       public HostStatusProvider::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<HostDeviceTimestampManager> Create(
        HostStatusProvider* host_status_provider,
        PrefService* pref_service,
        base::Clock* clock);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<HostDeviceTimestampManager> CreateInstance(
        HostStatusProvider* host_status_provider,
        PrefService* pref_service,
        base::Clock* clock) = 0;

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  HostDeviceTimestampManagerImpl(const HostDeviceTimestampManagerImpl&) =
      delete;
  HostDeviceTimestampManagerImpl& operator=(
      const HostDeviceTimestampManagerImpl&) = delete;

  ~HostDeviceTimestampManagerImpl() override;

  // HostDeviceTimestampManager:
  bool WasHostSetFromThisChromebook() override;
  std::optional<base::Time> GetLatestSetupFlowCompletionTimestamp() override;
  std::optional<base::Time> GetLatestVerificationTimestamp() override;

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

  raw_ptr<HostStatusProvider> host_status_provider_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<base::Clock> clock_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_DEVICE_TIMESTAMP_MANAGER_IMPL_H_
