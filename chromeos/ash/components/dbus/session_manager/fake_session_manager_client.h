// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SESSION_MANAGER_FAKE_SESSION_MANAGER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SESSION_MANAGER_FAKE_SESSION_MANAGER_CLIENT_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/arc/arc.pb.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace ash {

// A fake implementation of session_manager. Accepts policy blobs to be set and
// returns them unmodified.
class COMPONENT_EXPORT(SESSION_MANAGER) FakeSessionManagerClient
    : public SessionManagerClient {
 public:
  enum class PolicyStorageType {
    kOnDisk,    // Store policy in regular files on disk. Usually used for
                // fake D-Bus client implementation, see
                // SessionManagerClient::Create().
    kInMemory,  // Store policy in memory only. Usually used for tests.
  };

  enum class ServerBackedStateKeysHandling {
    // session_manager responds with configured state keys.
    kRegular,
    // session_manager responds with no state keys being available.
    kForceNotAvailable,
    // session_manager does not respond on GetServerBackedStateKeys.
    kNoResponse,
  };

  // A callback tht FakeSessionManagerClient can use to inform the test that
  // LoadShillProfile has been called.
  using OnLoadShillProfileCallback = base::RepeatingCallback<void(
      const cryptohome::AccountIdentifier& cryptohome_id)>;

  // Constructs a FakeSessionManagerClient with PolicyStorageType == kInMemory.
  // NOTE: This is different from SessionManagerClient::InitializeFake which
  // constructs an instance with PolicyStorageType == kOnDisk. Use
  // SessionManagerClient::InitializeFakeInMemory when replacing this.
  FakeSessionManagerClient();

  explicit FakeSessionManagerClient(PolicyStorageType policy_storage);

  FakeSessionManagerClient(const FakeSessionManagerClient&) = delete;
  FakeSessionManagerClient& operator=(const FakeSessionManagerClient&) = delete;

  ~FakeSessionManagerClient() override;

  // Returns the fake global instance if initialized. May return null.
  static FakeSessionManagerClient* Get();

  // SessionManagerClient overrides
  void SetStubDelegate(StubDelegate* delegate) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  bool IsScreenLocked() const override;
  void EmitLoginPromptVisible() override;
  void EmitAshInitialized() override;
  void RestartJob(int socket_fd,
                  const std::vector<std::string>& argv,
                  RestartJobReason reason,
                  chromeos::VoidDBusMethodCallback callback) override;
  void SaveLoginPassword(const std::string& password) override;

  void LoginScreenStorageStore(
      const std::string& key,
      const login_manager::LoginScreenStorageMetadata& metadata,
      const std::string& data,
      LoginScreenStorageStoreCallback callback) override;
  void LoginScreenStorageRetrieve(
      const std::string& key,
      LoginScreenStorageRetrieveCallback callback) override;
  void LoginScreenStorageListKeys(
      LoginScreenStorageListKeysCallback callback) override;
  void LoginScreenStorageDelete(const std::string& key) override;

  void StartSession(
      const cryptohome::AccountIdentifier& cryptohome_id) override;
  void StartSessionEx(const cryptohome::AccountIdentifier& cryptohome_id,
                      bool chrome_side_key_generation) override;
  void EmitStartedUserSession(
      const cryptohome::AccountIdentifier& cryptohome_id) override;
  void StopSession(login_manager::SessionStopReason reason) override;
  void LoadShillProfile(
      const cryptohome::AccountIdentifier& cryptohome_id) override;
  void StartDeviceWipe(chromeos::VoidDBusMethodCallback callback) override;
  void StartRemoteDeviceWipe(
      const enterprise_management::SignedData& signed_command) override;
  void ClearForcedReEnrollmentVpd(
      chromeos::VoidDBusMethodCallback callback) override;
  void UnblockDevModeForEnrollment(
      chromeos::VoidDBusMethodCallback callback) override;
  void UnblockDevModeForInitialStateDetermination(
      chromeos::VoidDBusMethodCallback callback) override;
  void UnblockDevModeForCarrierLock(
      chromeos::VoidDBusMethodCallback callback) override;
  void StartTPMFirmwareUpdate(const std::string& update_mode) override;
  void RequestLockScreen() override;
  void NotifyLockScreenShown() override;
  void NotifyLockScreenDismissed() override;
  bool BlockingRequestBrowserDataMigration(
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& mode) override;
  bool BlockingRequestBrowserDataBackwardMigration(
      const cryptohome::AccountIdentifier& cryptohome_id) override;
  void RetrieveActiveSessions(ActiveSessionsCallback callback) override;
  void RetrievePolicy(const login_manager::PolicyDescriptor& descriptor,
                      RetrievePolicyCallback callback) override;
  RetrievePolicyResponseType BlockingRetrievePolicy(
      const login_manager::PolicyDescriptor& descriptor,
      std::string* policy_out) override;
  void StoreDevicePolicy(const std::string& policy_blob,
                         chromeos::VoidDBusMethodCallback callback) override;
  void StorePolicyForUser(const cryptohome::AccountIdentifier& cryptohome_id,
                          const std::string& policy_blob,
                          chromeos::VoidDBusMethodCallback callback) override;
  void StoreDeviceLocalAccountPolicy(
      const std::string& account_id,
      const std::string& policy_blob,
      chromeos::VoidDBusMethodCallback callback) override;
  void StorePolicy(const login_manager::PolicyDescriptor& descriptor,
                   const std::string& policy_blob,
                   chromeos::VoidDBusMethodCallback callback) override;
  bool SupportsBrowserRestart() const override;
  void SetFlagsForUser(const cryptohome::AccountIdentifier& cryptohome_id,
                       const std::vector<std::string>& flags) override;
  void SetFeatureFlagsForUser(
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::vector<std::string>& feature_flags,
      const std::map<std::string, std::string>& origin_list_flags) override;
  void GetServerBackedStateKeys(StateKeysCallback callback) override;
  void GetPsmDeviceActiveSecret(
      PsmDeviceActiveSecretCallback callback) override;

  void StartArcMiniContainer(
      const arc::StartArcMiniInstanceRequest& request,
      chromeos::VoidDBusMethodCallback callback) override;
  void UpgradeArcContainer(const arc::UpgradeArcContainerRequest& request,
                           chromeos::VoidDBusMethodCallback callback) override;
  void StopArcInstance(const std::string& account_id,
                       bool should_backup_log,
                       chromeos::VoidDBusMethodCallback callback) override;
  void SetArcCpuRestriction(
      login_manager::ContainerCpuRestrictionState restriction_state,
      chromeos::VoidDBusMethodCallback callback) override;
  void EmitArcBooted(const cryptohome::AccountIdentifier& cryptohome_id,
                     chromeos::VoidDBusMethodCallback callback) override;
  void GetArcStartTime(
      chromeos::DBusMethodCallback<base::TimeTicks> callback) override;
  void EnableAdbSideload(EnableAdbSideloadCallback callback) override;
  void QueryAdbSideload(QueryAdbSideloadCallback callback) override;

  // Notifies observers as if ArcInstanceStopped signal is received.
  void NotifyArcInstanceStopped(login_manager::ArcContainerStopReason reason);

  // Returns true if flags for |cryptohome_id| have been set. If the return
  // value is |true|, |*out_flags_for_user| is filled with the flags passed to
  // |SetFlagsForUser|.
  bool GetFlagsForUser(const cryptohome::AccountIdentifier& cryptohome_id,
                       std::vector<std::string>* out_flags_for_user) const;

  // Notify observers about the session stopping.
  void NotifySessionStopping() const;

  // Configures state key retrieval error used to satisfy
  // GetServerBackedStateKeys() requests. Only available for
  // PolicyStorageType::kInMemory.
  void SetServerBackedStateKeyError(const StateKeyErrorType error_type);

  // Sets whether FakeSessionManagerClient should advertise (through
  // |SupportsBrowserRestart|) that it supports restarting Chrome. For example,
  // to apply user-session flags, or to start guest session.
  // The default is |false|.
  void set_supports_browser_restart(bool supports_browser_restart) {
    supports_browser_restart_ = supports_browser_restart;
  }

  // Requires set_support_restart_job() to be called.
  void set_restart_job_callback(base::OnceClosure callback) {
    restart_job_callback_ = std::move(callback);
  }

  const std::optional<std::vector<std::string>>& restart_job_argv() const {
    return restart_job_argv_;
  }

  std::optional<RestartJobReason> restart_job_reason() const {
    return restart_job_reason_;
  }

  void set_stop_session_callback(base::OnceClosure callback) {
    stop_session_callback_ = std::move(callback);
  }

  // If |force_failure| is true, forces StorePolicy() to fail.
  void ForceStorePolicyFailure(bool force_failure) {
    force_store_policy_failure_ = force_failure;
  }

  // If |force_load_error| is true, forces RetrievePolicy() to succeed with an
  // empty policy blob. This simulates a policy load error in session manager.
  void ForceRetrievePolicyLoadError(bool force_load_error) {
    force_retrieve_policy_load_error_ = force_load_error;
  }

  // Accessors for device policy. Only available for
  // PolicyStorageType::kInMemory.
  const std::string& device_policy() const;
  void set_device_policy(const std::string& policy_blob);

  // Accessors for user policy. Only available for PolicyStorageType::kInMemory.
  const std::string& user_policy(
      const cryptohome::AccountIdentifier& cryptohome_id) const;
  void set_user_policy(const cryptohome::AccountIdentifier& cryptohome_id,
                       const std::string& policy_blob);

  // Accessors for device local account policy. Only available for
  // PolicyStorageType::kInMemory.
  const std::string& device_local_account_policy(
      const std::string& account_id) const;
  void set_device_local_account_policy(const std::string& account_id,
                                       const std::string& policy_blob);

  const arc::UpgradeArcContainerRequest& last_upgrade_arc_request() const {
    return last_upgrade_arc_request_;
  }
  const arc::StartArcMiniInstanceRequest last_start_arc_mini_container_request()
      const {
    return last_start_arc_mini_container_request_;
  }

  // Notify observers about a property change completion.
  void OnPropertyChangeComplete(bool success);

  // Configures the list of state keys used to satisfy
  // GetServerBackedStateKeys() requests. Only available for
  // PolicyStorageType::kInMemory.
  void set_server_backed_state_keys(
      const std::vector<std::string>& state_keys) {
    DCHECK_EQ(policy_storage_, PolicyStorageType::kInMemory);
    server_backed_state_keys_ = state_keys;
  }

  void set_psm_device_active_secret(
      const std::string& psm_device_active_secret) {
    psm_device_active_secret_ = psm_device_active_secret;
  }

  int clear_forced_re_enrollment_vpd_call_count() const {
    return clear_forced_re_enrollment_vpd_call_count_;
  }

  int unblock_dev_mode_enrollment_call_count() const {
    return unblock_dev_mode_enrollment_call_count_;
  }

  int unblock_dev_mode_init_state_call_count() const {
    return unblock_dev_mode_init_state_call_count_;
  }

  int unblock_dev_mode_carrier_lock_call_count() const {
    return unblock_dev_mode_carrier_lock_call_count_;
  }

  void set_on_start_device_wipe_callback(base::OnceClosure callback);
  int start_device_wipe_call_count() const {
    return start_device_wipe_call_count_;
  }
  int request_lock_screen_call_count() const {
    return request_lock_screen_call_count_;
  }

  // Returns how many times LockScreenShown() was called.
  int notify_lock_screen_shown_call_count() const {
    return notify_lock_screen_shown_call_count_;
  }

  // Returns how many times LockScreenDismissed() was called.
  int notify_lock_screen_dismissed_call_count() const {
    return notify_lock_screen_dismissed_call_count_;
  }

  int start_tpm_firmware_update_call_count() const {
    return start_tpm_firmware_update_call_count_;
  }

  const std::string& last_tpm_firmware_update_mode() const {
    return last_tpm_firmware_update_mode_;
  }

  void set_arc_available(bool available) { arc_available_ = available; }
  void set_force_upgrade_failure(bool force_upgrade_failure) {
    force_upgrade_failure_ = force_upgrade_failure;
  }
  void set_arc_start_time(base::TimeTicks arc_start_time) {
    arc_start_time_ = arc_start_time;
  }

  void set_state_keys_handling(
      ServerBackedStateKeysHandling state_keys_handling) {
    state_keys_handling_ = state_keys_handling;
  }

  void set_adb_sideload_enabled(bool adb_sideload_enabled) {
    adb_sideload_enabled_ = adb_sideload_enabled;
  }

  void set_adb_sideload_response(AdbSideloadResponseCode response) {
    adb_sideload_response_ = response;
  }

  void set_on_load_shill_profile_callback(OnLoadShillProfileCallback callback) {
    on_load_shill_profile_callback_ = std::move(callback);
  }

  bool session_stopped() const { return session_stopped_; }

  const SessionManagerClient::ActiveSessionsMap& user_sessions() const {
    return user_sessions_;
  }

  const std::string& login_password() const { return login_password_; }

  const std::optional<std::string>& primary_user_id() const {
    return primary_user_id_;
  }

  bool request_browser_data_migration_called() const {
    return request_browser_data_migration_called_;
  }

  bool request_browser_data_migration_mode_called() const {
    return request_browser_data_migration_mode_called_;
  }

  const std::string& request_browser_data_migration_mode_value() const {
    return request_browser_data_migration_mode_value_;
  }

  bool request_browser_data_backward_migration_called() const {
    return request_browser_data_backward_migration_called_;
  }

 private:
  // Called in response to writing owner key file specified in new device
  // policy - used for in-memory fake only.
  // Notifies OwnerKeySet() observers, and runs |callback_to_run|.
  void HandleOwnerKeySet(base::OnceClosure callback_to_run);

  // Whether browser restarts should be handled - intended for use in tests.
  bool supports_browser_restart_ = false;

  // Callback that will be run, if set, when RestartJob() is called.
  base::OnceClosure restart_job_callback_;

  // If restart job was requested, and the client supports restart job, the
  // requested restarted arguments.
  std::optional<std::vector<std::string>> restart_job_argv_;

  // If restart job was requested, and the client supports restart job, the
  // requested restart reason.
  std::optional<RestartJobReason> restart_job_reason_;

  // Callback that will be run, if set, when StopSession() is called.
  base::OnceClosure stop_session_callback_;

  base::ObserverList<Observer>::UncheckedAndDanglingUntriaged observers_{
      SessionManagerClient::kObserverListPolicy};
  SessionManagerClient::ActiveSessionsMap user_sessions_;
  base::expected<std::vector<std::string>, StateKeyErrorType>
      server_backed_state_keys_;

  std::string psm_device_active_secret_;

  // Policy is stored in |policy_| if |policy_storage_| type is
  // PolicyStorageType::kInMemory. Uses the relative stub file path as key.
  const PolicyStorageType policy_storage_;
  std::map<std::string, std::string> policy_;

  // If set to true, StorePolicy() always fails.
  bool force_store_policy_failure_ = false;

  // If set to true, RetrievePolicy() always succeeds with an empty policy blob.
  // This simulates a policy load error in session manager.
  bool force_retrieve_policy_load_error_ = false;

  // The response that will be returned when QueryAdbSideload() is called.
  AdbSideloadResponseCode adb_sideload_response_ =
      AdbSideloadResponseCode::SUCCESS;

  int clear_forced_re_enrollment_vpd_call_count_ = 0;
  int unblock_dev_mode_enrollment_call_count_ = 0;
  int unblock_dev_mode_init_state_call_count_ = 0;
  int unblock_dev_mode_carrier_lock_call_count_ = 0;
  // Callback which is run after calling |StartDeviceWipe| or
  // |StartRemoteDeviceWipe|.
  base::OnceClosure on_start_device_wipe_callback_;
  int start_device_wipe_call_count_ = 0;
  int request_lock_screen_call_count_ = 0;
  int notify_lock_screen_shown_call_count_ = 0;
  int notify_lock_screen_dismissed_call_count_ = 0;
  int start_tpm_firmware_update_call_count_ = 0;
  std::string last_tpm_firmware_update_mode_;
  bool screen_is_locked_ = false;
  ServerBackedStateKeysHandling state_keys_handling_ =
      ServerBackedStateKeysHandling::kRegular;
  OnLoadShillProfileCallback on_load_shill_profile_callback_;

  bool arc_available_ = false;
  bool force_upgrade_failure_ = false;
  base::TimeTicks arc_start_time_;

  bool container_running_ = false;

  bool adb_sideload_enabled_ = false;

  std::string login_password_;

  bool request_browser_data_migration_called_ = false;
  bool request_browser_data_migration_mode_called_ = false;
  std::string request_browser_data_migration_mode_value_ = "invalid";

  bool request_browser_data_backward_migration_called_ = false;

  // Contains last request passed to StartArcMiniContainer
  arc::StartArcMiniInstanceRequest last_start_arc_mini_container_request_;

  // Contains last request passed to StartArcInstance
  arc::UpgradeArcContainerRequest last_upgrade_arc_request_;

  raw_ptr<StubDelegate> delegate_ = nullptr;

  bool session_stopped_ = false;

  // The flags and feature flags state that has been set for a user through
  // |SetFlagsForUser| and |SetFeatureFlagsForUser|.
  struct FlagsState {
    FlagsState();
    ~FlagsState();

    std::vector<std::string> flags;
    std::vector<std::string> feature_flags;
    std::map<std::string, std::string> origin_list_flags;
  };
  std::map<cryptohome::AccountIdentifier, FlagsState> flags_for_user_;

  std::optional<std::string> primary_user_id_;

  base::flat_map<std::string, std::string> login_screen_storage_;

  base::flat_set<base::FilePath> files_to_clean_up_;

  base::WeakPtrFactory<FakeSessionManagerClient> weak_ptr_factory_{this};
};

// Helper class to create FakeSessionManagerClient. Note that the existing
// SessionManagerClient instance will be released.
class COMPONENT_EXPORT(SESSION_MANAGER) ScopedFakeSessionManagerClient {
 public:
  ScopedFakeSessionManagerClient();
  explicit ScopedFakeSessionManagerClient(
      FakeSessionManagerClient::PolicyStorageType policy_storage);
  ~ScopedFakeSessionManagerClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SESSION_MANAGER_FAKE_SESSION_MANAGER_CLIENT_H_
