// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/multidevice_setup/account_status_change_delegate_notifier.h"
#include "chromeos/ash/services/multidevice_setup/host_status_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/oobe_completion_tracker.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
}  // namespace base

namespace ash {

namespace multidevice_setup {

class HostDeviceTimestampManager;

// Concrete AccountStatusChangeDelegateNotifier implementation, which uses
// HostStatusProvider to check for account changes and PrefStore to track
// previous notifications.
class AccountStatusChangeDelegateNotifierImpl
    : public AccountStatusChangeDelegateNotifier,
      public HostStatusProvider::Observer,
      public OobeCompletionTracker::Observer,
      public session_manager::SessionManagerObserver {
 public:
  class Factory {
   public:
    static std::unique_ptr<AccountStatusChangeDelegateNotifier> Create(
        HostStatusProvider* host_status_provider,
        PrefService* pref_service,
        HostDeviceTimestampManager* host_device_timestamp_manager,
        OobeCompletionTracker* oobe_completion_tracker,
        base::Clock* clock);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<AccountStatusChangeDelegateNotifier> CreateInstance(
        HostStatusProvider* host_status_provider,
        PrefService* pref_service,
        HostDeviceTimestampManager* host_device_timestamp_manager,
        OobeCompletionTracker* oobe_completion_tracker,
        base::Clock* clock) = 0;

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  AccountStatusChangeDelegateNotifierImpl(
      const AccountStatusChangeDelegateNotifierImpl&) = delete;
  AccountStatusChangeDelegateNotifierImpl& operator=(
      const AccountStatusChangeDelegateNotifierImpl&) = delete;

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

  //   static const char kMultiDeviceShowSetupNotificationNextUnlock[];
  static const char kMultiDeviceLastSessionStartTime[];

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

  // SessionManagerObserver::
  void OnSessionStateChanged() override;

  void UpdateSessionStartTimeIfEligible();

  bool IsInPhoneHubNotificationExperimentGroup();

  void CheckForMultiDeviceEvents(
      const HostStatusProvider::HostStatusWithDevice& host_status_with_device);

  void CheckForNewUserPotentialHostExistsEvent(
      const HostStatusProvider::HostStatusWithDevice& host_status_with_device);
  void CheckForNoLongerNewUserEvent(
      const HostStatusProvider::HostStatusWithDevice& host_status_with_device,
      const std::optional<mojom::HostStatus> host_status_before_update);
  void CheckForExistingUserHostSwitchedEvent(
      const HostStatusProvider::HostStatusWithDevice& host_status_with_device,
      const std::optional<std::string>& verified_host_device_id_before_update);
  void CheckForExistingUserChromebookAddedEvent(
      const HostStatusProvider::HostStatusWithDevice& host_status_with_device,
      const std::optional<std::string>& verified_host_device_id_before_update);

  // Loads data from previous session using PrefService.
  std::optional<std::string> LoadHostDeviceIdFromEndOfPreviousSession();

  // Set to std::nullopt if there was no enabled host in the most recent
  // host status update.
  std::optional<std::string> verified_host_device_id_from_most_recent_update_;

  // Set to std::nullopt until the first host status update.
  std::optional<mojom::HostStatus> host_status_from_most_recent_update_;

  mojo::Remote<mojom::AccountStatusChangeDelegate> delegate_remote_;
  raw_ptr<HostStatusProvider> host_status_provider_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<HostDeviceTimestampManager> host_device_timestamp_manager_;
  raw_ptr<OobeCompletionTracker> oobe_completion_tracker_;
  raw_ptr<base::Clock> clock_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_IMPL_H_
