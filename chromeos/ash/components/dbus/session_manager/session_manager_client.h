// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SESSION_MANAGER_SESSION_MANAGER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SESSION_MANAGER_SESSION_MANAGER_CLIENT_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "third_party/cros_system_api/dbus/login_manager/dbus-constants.h"

namespace arc {
class StartArcMiniInstanceRequest;
class UpgradeArcContainerRequest;
}  // namespace arc

namespace cryptohome {
class AccountIdentifier;
}

namespace dbus {
class Bus;
}

namespace enterprise_management {
class SignedData;
}

namespace login_manager {
class LoginScreenStorageMetadata;
class PolicyDescriptor;
}  // namespace login_manager

namespace ash {

// SessionManagerClient is used to communicate with the session manager.
class COMPONENT_EXPORT(SESSION_MANAGER) SessionManagerClient {
 public:
  static constexpr base::ObserverListPolicy kObserverListPolicy =
      base::ObserverListPolicy::EXISTING_ONLY;

  // The result type received from session manager on request to retrieve the
  // policy. Used to define the buckets for an enumerated UMA histogram.
  // Hence,
  //   (a) existing enumerated constants should never be deleted or reordered.
  //   (b) new constants should be inserted immediately before COUNT.
  enum class RetrievePolicyResponseType {
    // Other type of error while retrieving policy data (e.g. D-Bus timeout).
    OTHER_ERROR = 0,
    // The policy was retrieved successfully.
    SUCCESS = 1,
    // Retrieve policy request issued before session started (deprecated, use
    // GET_SERVICE_FAIL).
    SESSION_DOES_NOT_EXIST_DEPRECATED = 2,
    // Session manager failed to encode the policy data.
    POLICY_ENCODE_ERROR = 3,
    // Session manager failed to get the policy service, possibly because a user
    // session hasn't started yet or the account id was invalid.
    GET_SERVICE_FAIL = 4,
    // Has to be the last value of enumeration. Used for UMA.
    COUNT
  };

  enum class AdbSideloadResponseCode {
    // ADB sideload operation has finished successfully.
    SUCCESS = 1,
    // ADB sideload operation has failed.
    FAILED = 2,
    // ADB sideload requires a powerwash to unblock (to define nvram).
    NEED_POWERWASH = 3,
  };

  enum class RestartJobReason : uint32_t {
    // Restart browser for Guest session.
    kGuest = 0,
    // Restart browser without user session for headless Chromium.
    kUserless = 1,
  };

  // Error type encountered while retrieving state keys. These values are
  // persisted to logs. Entries should not be renumbered and numeric values
  // should never be reused.
  enum class StateKeyErrorType {
    kNoError = 0,
    kInvalidResponse = 1,
    kCommunicationError = 2,
    kMissingIdentifiers = 3,
    kMaxValue = kMissingIdentifiers
  };

  // Interface for observing changes from the session manager.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the owner key is set.
    virtual void OwnerKeySet(bool success) {}

    // Called when the property change is complete.
    virtual void PropertyChangeComplete(bool success) {}

    // Called after EmitLoginPromptVisible is called.
    virtual void EmitLoginPromptVisibleCalled() {}

    // Called when the ARC instance is stopped after it had already started.
    virtual void ArcInstanceStopped(
        login_manager::ArcContainerStopReason reason) {}

    // Called when screen lock state is updated.
    virtual void ScreenLockedStateUpdated() {}

    // Called when a powerwash is requested.
    virtual void PowerwashRequested(bool admin_requested) {}

    // Called when session stopping signal is received
    virtual void SessionStopping() {}
  };

  // Interface for performing actions on behalf of the stub implementation.
  class StubDelegate {
   public:
    virtual ~StubDelegate() {}

    // Locks the screen. Invoked by the stub when RequestLockScreen() is called.
    // In the real implementation of SessionManagerClient::RequestLockScreen(),
    // a lock request is forwarded to the session manager; in the stub, this is
    // short-circuited and the screen is locked immediately.
    virtual void LockScreenForStub() = 0;
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Creates and initializes an InMemory fake global instance for testing.
  static void InitializeFakeInMemory();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static SessionManagerClient* Get();

  SessionManagerClient(const SessionManagerClient&) = delete;
  SessionManagerClient& operator=(const SessionManagerClient&) = delete;

  // Sets the delegate used by the stub implementation. Ownership of |delegate|
  // remains with the caller.
  virtual void SetStubDelegate(StubDelegate* delegate) = 0;

  // Adds or removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool HasObserver(const Observer* observer) const = 0;

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) = 0;

  // Returns the most recent screen-lock state received from session_manager.
  // This method should only be called by low-level code that is unable to
  // depend on UI code and get the lock state from it instead.
  virtual bool IsScreenLocked() const = 0;

  // Kicks off an attempt to emit the "login-prompt-visible" upstart signal.
  virtual void EmitLoginPromptVisible() = 0;

  // Kicks off an attempt to emit the "ash-initialized" upstart signal.
  virtual void EmitAshInitialized() = 0;

  // Restarts the browser job, passing |argv| as the updated command line.
  // The session manager requires a RestartJob caller to open a socket pair and
  // pass one end while holding the local end open for the duration of the call.
  // The session manager uses this to determine whether the PID the restart
  // request originates from belongs to the browser itself.
  // This method duplicates |socket_fd| so it's OK to close the FD without
  // waiting for the result.
  // |reason| - restart job without user session (for headless chromium)
  // or with user session (for guest sessions only).
  virtual void RestartJob(int socket_fd,
                          const std::vector<std::string>& argv,
                          RestartJobReason reason,
                          chromeos::VoidDBusMethodCallback callback) = 0;

  // Sends the user's password to the session manager.
  virtual void SaveLoginPassword(const std::string& password) = 0;

  // Used to report errors from |LoginScreenStorageStore()|. |error| should
  // contain an error message if an error occurred.
  using LoginScreenStorageStoreCallback =
      chromeos::DBusMethodCallback<std::string /* error */>;

  // Stores data to the login screen storage. login screen storage is a D-Bus
  // API that is used by the custom login screen implementations to inject
  // credentials into the session and store persistent data across the login
  // screen restarts.
  virtual void LoginScreenStorageStore(
      const std::string& key,
      const login_manager::LoginScreenStorageMetadata& metadata,
      const std::string& data,
      LoginScreenStorageStoreCallback callback) = 0;

  // Used for |LoginScreenStorageRetrieve()| method. |data| argument is the data
  // returned by the session manager. |error| contains an error message if an
  // error occurred, otherwise empty.
  using LoginScreenStorageRetrieveCallback =
      base::OnceCallback<void(std::optional<std::string> /* data */,
                              std::optional<std::string> /* error */)>;

  // Retrieve data stored earlier with the |LoginScreenStorageStore()| method.
  virtual void LoginScreenStorageRetrieve(
      const std::string& key,
      LoginScreenStorageRetrieveCallback callback) = 0;

  // Used for |LoginScreenStorageListKeys()| method. |keys| argument is the list
  // of keys currently stored in the login screen storage. In case of error,
  // |keys| is empty and |error| contains the error message.
  using LoginScreenStorageListKeysCallback =
      base::OnceCallback<void(std::vector<std::string> /* keys */,
                              std::optional<std::string> /* error */)>;

  // List all keys currently stored in the login screen storage.
  virtual void LoginScreenStorageListKeys(
      LoginScreenStorageListKeysCallback callback) = 0;

  // Delete a key and the value associated with it from the login screen
  // storage.
  virtual void LoginScreenStorageDelete(const std::string& key) = 0;

  // Starts the session for the user.
  virtual void StartSession(
      const cryptohome::AccountIdentifier& cryptohome_id) = 0;
  // Same as |StartSession|, but also tells session_manager whether Chrome will
  // handle owner key generation or not (based on |chrome_side_key_generation|).
  virtual void StartSessionEx(
      const cryptohome::AccountIdentifier& cryptohome_id,
      bool chrome_side_key_generation) = 0;

  // Emits the "started-user-session" upstart signal to notify all the critical
  // login tasks are completed.
  virtual void EmitStartedUserSession(
      const cryptohome::AccountIdentifier& cryptohome_id) = 0;

  // Stops the current session. Don't call directly unless there's no user on
  // the device. Use SessionTerminationManager::StopSession instead.
  virtual void StopSession(login_manager::SessionStopReason reason) = 0;

  // Triggers loading the shill profile for |cryptohome_id|.
  virtual void LoadShillProfile(
      const cryptohome::AccountIdentifier& cryptohome_id) = 0;

  // Starts the factory reset.
  virtual void StartDeviceWipe(chromeos::VoidDBusMethodCallback callback) = 0;

  // Starts a remotely initiated factory reset, similar to |StartDeviceWipe|
  // above, but also performs additional checks on Chrome OS side.
  // session_manager validates |signed_command| against SHA256_RSA.
  virtual void StartRemoteDeviceWipe(
      const enterprise_management::SignedData& signed_command) = 0;

  // Set the block_demode and check_enrollment flags to 0 in the VPD.
  virtual void ClearForcedReEnrollmentVpd(
      chromeos::VoidDBusMethodCallback callback) = 0;

  virtual void UnblockDevModeForEnrollment(
      chromeos::VoidDBusMethodCallback callback) = 0;

  virtual void UnblockDevModeForInitialStateDetermination(
      chromeos::VoidDBusMethodCallback callback) = 0;

  virtual void UnblockDevModeForCarrierLock(
      chromeos::VoidDBusMethodCallback callback) = 0;

  // Triggers a TPM firmware update.
  virtual void StartTPMFirmwareUpdate(const std::string& update_mode) = 0;

  // Sends a request to lock the screen to session_manager. Locking occurs
  // asynchronously.
  virtual void RequestLockScreen() = 0;

  // Notifies session_manager that Chrome has shown the lock screen.
  virtual void NotifyLockScreenShown() = 0;

  // Notifies session_manager that Chrome has hidden the lock screen.
  virtual void NotifyLockScreenDismissed() = 0;

  // Makes session_manager add some flags to carry out browser data migration
  // upon next ash-chrome restart. The method returns true if the DBus call was
  // successful. The callback is passed true if the DBus call is successful and
  // false otherwise.
  // This method is blocking. Do not use unless necessary.
  virtual bool BlockingRequestBrowserDataMigration(
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& mode) = 0;

  // Makes session_manager add some flags to carry out browser data backward
  // migration upon next ash-chrome restart. The method returns true if the DBus
  // call was successful. The callback is passed true if the DBus call is
  // successful and false otherwise.
  // This method is blocking. Do not use unless necessary.
  virtual bool BlockingRequestBrowserDataBackwardMigration(
      const cryptohome::AccountIdentifier& cryptohome_id) = 0;

  // Map that is used to describe the set of active user sessions where |key|
  // is cryptohome id and |value| is user_id_hash.
  using ActiveSessionsMap = std::map<std::string, std::string>;

  // The ActiveSessionsCallback is used for the RetrieveActiveSessions()
  // method. It receives |sessions| argument where the keys are cryptohome_ids
  // for all users that are currently active.
  using ActiveSessionsCallback =
      chromeos::DBusMethodCallback<ActiveSessionsMap /* sessions */>;

  // Enumerates active user sessions. Usually Chrome naturally keeps track of
  // active users when they are added into current session. When Chrome is
  // restarted after crash by session_manager it only receives cryptohome id and
  // user_id_hash for one user. This method is used to retrieve list of all
  // active users.
  virtual void RetrieveActiveSessions(ActiveSessionsCallback callback) = 0;

  // TODO(crbug.com/41344863): Change the policy storage interface so that it
  // has a single StorePolicy, RetrievePolicy, BlockingRetrivePolicy method that
  // takes a PolicyDescriptor.

  // Used for RetrieveDevicePolicy, RetrievePolicyForUser and
  // RetrieveDeviceLocalAccountPolicy. Takes a serialized protocol buffer as
  // string.  Upon success, we will pass a protobuf and SUCCESS |response_type|
  // to the callback. On failure, we will pass "" and the details of error type
  // in |response_type|.
  using RetrievePolicyCallback =
      base::OnceCallback<void(RetrievePolicyResponseType response_type,
                              const std::string& protobuf)>;

  // Fetches a policy blob stored by the session manager. Invokes |callback|
  // upon completion.
  virtual void RetrievePolicy(const login_manager::PolicyDescriptor& descriptor,
                              RetrievePolicyCallback callback) = 0;

  // Same as RetrievePolicy() but blocks until a reply is
  // received, and populates the policy synchronously. Returns SUCCESS when
  // successful, or the corresponding error from enum in case of a failure.
  // This may only be called in situations where blocking the UI thread is
  // considered acceptable (e.g. restarting the browser after a crash or after
  // a flag change).
  // TODO(crbug.com/40296212): Get rid of blocking calls.
  virtual RetrievePolicyResponseType BlockingRetrievePolicy(
      const login_manager::PolicyDescriptor& descriptor,
      std::string* policy_out) = 0;

  // Attempts to asynchronously store |policy_blob| as device policy.  Upon
  // completion of the store attempt, we will call callback.
  virtual void StoreDevicePolicy(const std::string& policy_blob,
                                 chromeos::VoidDBusMethodCallback callback) = 0;

  // Attempts to asynchronously store |policy_blob| as user policy for the
  // given |cryptohome_id|. Upon completion of the store attempt, we will call
  // callback.
  virtual void StorePolicyForUser(
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& policy_blob,
      chromeos::VoidDBusMethodCallback callback) = 0;

  // Sends a request to store a policy blob for the specified device-local
  // account. The result of the operation is reported through |callback|.
  virtual void StoreDeviceLocalAccountPolicy(
      const std::string& account_id,
      const std::string& policy_blob,
      chromeos::VoidDBusMethodCallback callback) = 0;

  // Sends a request to store a |policy_blob| to Session Manager. The storage
  // location is determined by |descriptor|. The result of the operation is
  // reported through |callback|.
  virtual void StorePolicy(const login_manager::PolicyDescriptor& descriptor,
                           const std::string& policy_blob,
                           chromeos::VoidDBusMethodCallback callback) = 0;

  // Returns whether session manager can be used to restart Chrome in order to
  // apply per-user session flags, or start guest session.
  // This returns true for the real session manager client implementation, and
  // false for the fake (unless explicitly set by a test).
  virtual bool SupportsBrowserRestart() const = 0;

  // Sets the flags to be applied next time by the session manager when Chrome
  // is restarted inside an already started session for a particular user.
  virtual void SetFlagsForUser(
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::vector<std::string>& flags) = 0;

  // Sets feature flags to pass next time Chrome gets restarted by the session
  // manager.
  virtual void SetFeatureFlagsForUser(
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::vector<std::string>& feature_flags,
      const std::map<std::string, std::string>& origin_list_flags) = 0;

  using StateKeysCallback = base::OnceCallback<void(
      const base::expected<std::vector<std::string>, StateKeyErrorType>&
          state_keys)>;

  // Get the currently valid server-backed state keys for the device.
  // Server-backed state keys are opaque, device-unique, time-dependent,
  // client-determined identifiers that are used for keying state in the cloud
  // for the device to retrieve after a device factory reset.
  //
  // The state keys are returned asynchronously via |callback|. The callback
  // is invoked with an empty state key vector in case of errors. If the time
  // sync fails or there's no network, the callback is never invoked.
  virtual void GetServerBackedStateKeys(StateKeysCallback callback) = 0;

  using PsmDeviceActiveSecretCallback =
      base::OnceCallback<void(const std::string& psm_device_active_secret)>;

  // Get a derivative of the stable_device_secret_DO_NOT_SHARE vpd field.
  // Derivative of this field is used to prevent privacy complications in the
  // case of a Chrome data leak.
  //
  // The string is returned asynchronously via |callback|. The callback is
  // invoked with an empty string in case of errors.
  virtual void GetPsmDeviceActiveSecret(
      PsmDeviceActiveSecretCallback callback) = 0;

  // StartArcMiniContainer starts a container with only a handful of ARC
  // processes for Chrome OS login screen.
  virtual void StartArcMiniContainer(
      const arc::StartArcMiniInstanceRequest& request,
      chromeos::VoidDBusMethodCallback callback) = 0;

  // UpgradeArcContainer upgrades a mini-container to a full ARC container. On
  // upgrade failure, the container will be shutdown. The container shutdown
  // will trigger the ArcInstanceStopped signal (as usual). There are no
  // guarantees over whether this |callback| is invoked or the
  // ArcInstanceStopped signal is received first.
  virtual void UpgradeArcContainer(
      const arc::UpgradeArcContainerRequest& request,
      chromeos::VoidDBusMethodCallback callback) = 0;

  // Asynchronously stops the ARC instance. When |should_backup_log| is set to
  // true it also initiates ARC log back up operation on debugd for the given
  // |account_id|. Upon completion, invokes |callback| with the result;
  // true on success, false on failure (either session manager failed to
  // stop an instance or session manager can not be reached).
  virtual void StopArcInstance(const std::string& account_id,
                               bool should_backup_log,
                               chromeos::VoidDBusMethodCallback callback) = 0;

  // Adjusts the amount of CPU the ARC instance is allowed to use. When
  // |restriction_state| is CONTAINER_CPU_RESTRICTION_FOREGROUND the limit is
  // adjusted so ARC can use all the system's CPU if needed. When it is
  // CONTAINER_CPU_RESTRICTION_BACKGROUND, ARC can only use tightly restricted
  // CPU resources. The ARC instance is started in a state that is more
  // restricted than CONTAINER_CPU_RESTRICTION_BACKGROUND. When ARC is not
  // supported, the function asynchronously runs the |callback| with false.
  virtual void SetArcCpuRestriction(
      login_manager::ContainerCpuRestrictionState restriction_state,
      chromeos::VoidDBusMethodCallback callback) = 0;

  // Emits the "arc-booted" upstart signal.
  virtual void EmitArcBooted(const cryptohome::AccountIdentifier& cryptohome_id,
                             chromeos::VoidDBusMethodCallback callback) = 0;

  // Asynchronously retrieves the timestamp which ARC instance is invoked.
  // Returns nullopt if there is no ARC instance or ARC is not available.
  virtual void GetArcStartTime(
      chromeos::DBusMethodCallback<base::TimeTicks> callback) = 0;

  using EnableAdbSideloadCallback =
      base::OnceCallback<void(AdbSideloadResponseCode response_code)>;

  // Asynchronously attempts to enable ARC APK Sideloading. Upon completion,
  // invokes |callback| with the result; true on success, false on failure of
  // any kind.
  virtual void EnableAdbSideload(EnableAdbSideloadCallback callback) = 0;

  using QueryAdbSideloadCallback =
      base::OnceCallback<void(AdbSideloadResponseCode response_code,
                              bool is_allowed)>;

  // Asynchronously queries for the current status of ARC APK Sideloading. Upon
  // completion, invokes |callback| with |succeeded| indicating if the query
  // could be completed. If |succeeded| is true, |is_allowed| contains the
  // current status of whether ARC APK Sideloading is allowed on this device,
  // based on previous explicit user opt-in.
  virtual void QueryAdbSideload(QueryAdbSideloadCallback callback) = 0;

 protected:
  // Use Initialize/Shutdown instead.
  SessionManagerClient();
  virtual ~SessionManagerClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SESSION_MANAGER_SESSION_MANAGER_CLIENT_H_
