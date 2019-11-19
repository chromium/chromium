// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chromeos/services/multidevice_setup/account_status_change_delegate_notifier.h"
#include "chromeos/services/multidevice_setup/host_status_provider.h"
#include "chromeos/services/multidevice_setup/public/cpp/oobe_completion_tracker.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
}  // namespace base

namespace chromeos {

namespace multidevice_setup {

class HostDeviceTimestampManager;

// Concrete AccountStatusChangeDelegateNotifier implementation, which uses
// HostStatusProvider to check for account changes and PrefStore to track
// previous notifications.
class AccountStatusChangeDelegateNotifierImpl
    : public AccountStatusChangeDelegateNotifier,
      public HostStatusProvider::Observer,
      public OobeCompletionTracker::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<AccountStatusChangeDelegateNotifier> BuildInstance(
        HostStatusProvider* host_status_provider,
        PrefService* pref_service,
        HostDeviceTimestampManager* host_device_timestamp_manager,
        OobeCompletionTracker* oobe_completion_tracker,
        base::Clock* clock);

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~AccountStatusChangeDelegateNotifierImpl() override;

  void SetAccountStatusChangeDelegateRemote(
      mojo::PendingRemote<mojom::AccountStatusChangeDelegate> delegate_remote);

 private:
  friend class MultiDeviceSetupAccountStatusChangeDelegateNotifierTest;

  static const char kNewUserPotentialHostExistsPrefName[];
  static const char kExistingUserHostSwitchedPrefName[];
  static const char kExistingUserChromebookAddedPrefName[];

  static const char kOobeSetupFlowTimestampPrefName[];
  static const char
      kVerifiedHostDeviceIdFromMostRecentHostStatusUpdatePrefName[];

  AccountStatusChangeDelegateNotifierImpl(
      HostStatusProvider* host_status_provider,
      PrefService* pref_service,
      HostDeviceTimestampManager* host_device_timestamp_manager,
      OobeCompletionTracker* oobe_completion_tracker,
      base::Clock* clock);

  // AccountStatusChangeDelegateNotifier:
  void OnDelegateSet() override;

  // HostStatusProvider::Observer:
  void OnHostStatusChange(const HostStatusProvider::HostStatusWithDevice&
                              host_status_with_device) override;

  // OobeCompletionTracker::Observer:
  void OnOobeCompleted() override;

  void CheckForMultiDeviceEvents(
      const HostStatusProvider::HostStatusWithDevice& host_status_with_device);

  void CheckForNewUserPotentialHostExistsEvent(
      const HostStatusProvider::HostStatusWithDevice& host_status_with_device);
  void CheckForNoLongerNewUserEvent(
      const HostStatusProvider::HostStatusWithDevice& host_status_with_device,
      const base::Optional<mojom::HostStatus> host_status_before_update);
  void CheckForExistingUserHostSwitchedEvent(
      const HostStatusProvider::HostStatusWithDevice& host_status_with_device,
      const base::Optional<std::string>& verified_host_device_id_before_update);
  void CheckForExistingUserChromebookAddedEvent(
      const HostStatusProvider::HostStatusWithDevice& host_status_with_device,
      const base::Optional<std::string>& verified_host_device_id_before_update);

  // Loads data from previous session using PrefService.
  base::Optional<std::string> LoadHostDeviceIdFromEndOfPreviousSession();

  // Set to base::nullopt if there was no enabled host in the most recent
  // host status update.
  base::Optional<std::string> verified_host_device_id_from_most_recent_update_;

  // Set to base::nullopt until the first host status update.
  base::Optional<mojom::HostStatus> host_status_from_most_recent_update_;

  mojo::Remote<mojom::AccountStatusChangeDelegate> delegate_remote_;
  HostStatusProvider* host_status_provider_;
  PrefService* pref_service_;
  HostDeviceTimestampManager* host_device_timestamp_manager_;
  OobeCompletionTracker* oobe_completion_tracker_;
  base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(AccountStatusChangeDelegateNotifierImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_IMPL_H_
