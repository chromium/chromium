// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"

#include <stddef.h>
#include <stdint.h>

#include <cstring>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/dbus/arc/arc.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/login_manager/login_screen_storage.pb.h"
#include "chromeos/ash/components/dbus/login_manager/policy_descriptor.pb.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/policy_descriptor.h"
#include "chromeos/dbus/common/blocking_method_caller.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "dbus/bus.h"
#include "dbus/error.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/login_manager/dbus-constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

SessionManagerClient* g_instance = nullptr;

using RetrievePolicyResponseType =
    SessionManagerClient::RetrievePolicyResponseType;

constexpr char kEmptyAccountId[] = "";
// The timeout used when starting the android container is 90 seconds
constexpr int kStartArcTimeout = 90 * 1000;

// TODO(b/205032502): Because upgrading the container from mini to full often
// takes more than 25 seconds, increasing it to 1 minute for now. Once we have
// the update metrics, update the timeout to a tighter value.
constexpr int kUpgradeTimeoutMs = 60 * 1000;  // 60 seconds

// 10MB. It's the current restriction enforced by session manager.
const size_t kSharedMemoryDataSizeLimit = 10 * 1024 * 1024;

// Copy of values from login_manager::SessionManagerImpl.
// TODO(crbug.com/40071048): Move to system_api/dbus/service_constants.h
constexpr char kStopping[] = "stopping";

// Helper to get the enum type of RetrievePolicyResponseType based on error
// name.
RetrievePolicyResponseType GetPolicyResponseTypeByError(
    std::string_view error_name) {
  if (error_name == login_manager::dbus_error::kNone) {
    return RetrievePolicyResponseType::SUCCESS;
  } else if (error_name == login_manager::dbus_error::kGetServiceFail ||
             error_name == login_manager::dbus_error::kSessionDoesNotExist) {
    // TODO(crbug.com/41344863): Remove kSessionDoesNotExist case once Chrome OS
    // has switched to kGetServiceFail.
    return RetrievePolicyResponseType::GET_SERVICE_FAIL;
  } else if (error_name == login_manager::dbus_error::kSigEncodeFail) {
    return RetrievePolicyResponseType::POLICY_ENCODE_ERROR;
  }
  return RetrievePolicyResponseType::OTHER_ERROR;
}

// Creates a pipe that contains the given data. The data will be prefixed by a
// size_t sized variable containing the size of the data to read. Since we don't
// pass this pipe's read end anywhere, we can be sure that the only FD that can
// read from that pipe will be closed on browser's exit, therefore the password
// won't be leaked if the browser crashes.
base::ScopedFD CreatePasswordPipe(const std::string& data) {
  // 64k of data, minus 64 bits for a preceding size. This number was chosen to
  // fit all the data in a single pipe buffer and avoid blocking on write.
  // (http://man7.org/linux/man-pages/man7/pipe.7.html)
  const size_t kPipeDataSizeLimit = 1024 * 64 - sizeof(size_t);

  int pipe_fds[2];
  if (data.size() > kPipeDataSizeLimit ||
      !base::CreateLocalNonBlockingPipe(pipe_fds)) {
    DLOG(ERROR) << "Failed to create pipe";
    return base::ScopedFD();
  }
  base::ScopedFD pipe_read_end(pipe_fds[0]);
  base::ScopedFD pipe_write_end(pipe_fds[1]);

  const size_t data_size = data.size();

  base::WriteFileDescriptor(pipe_write_end.get(),
                            base::byte_span_from_ref(data_size));
  base::WriteFileDescriptor(pipe_write_end.get(), data);

  return pipe_read_end;
}

// Creates a read-only shared memory region that contains the given data.
base::ScopedFD CreateSharedMemoryRegionFDWithData(const std::string& data) {
  if (data.size() > kSharedMemoryDataSizeLimit) {
    LOG(ERROR) << "Couldn't create shared memory, data is too big.";
    return base::ScopedFD();
  }
  auto region = base::WritableSharedMemoryRegion::Create(data.size());
  base::WritableSharedMemoryMapping mapping = region.Map();
  if (!mapping.IsValid())
    return base::ScopedFD();
  mapping.GetMemoryAsSpan<uint8_t>().copy_from(base::as_byte_span(data));
  return base::WritableSharedMemoryRegion::TakeHandleForSerialization(
             std::move(region))
      .PassPlatformHandle()
      .readonly_fd;
}

// Reads |secret_size| bytes from a given shared memory region. Puts result into
// |secret|. |fd| should point at a read-only shared memory region.
bool ReadSecretFromSharedMemory(base::ScopedFD fd,
                                size_t secret_size,
                                std::vector<uint8_t>* secret) {
  if (secret_size > kSharedMemoryDataSizeLimit) {
    LOG(ERROR) << "Couldn't read secret from the shared memory, "
                  "secret's size is too big.";
    return false;
  }
  auto platform_region(base::subtle::PlatformSharedMemoryRegion::Take(
      std::move(fd), base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly,
      secret_size, base::UnguessableToken::Create()));
  if (!platform_region.IsValid())
    return false;
  auto region =
      base::ReadOnlySharedMemoryRegion::Deserialize(std::move(platform_region));
  if (!region.IsValid())
    return false;
  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  if (!mapping.IsValid())
    return false;
  secret->resize(secret_size);
  memcpy(secret->data(), mapping.memory(), secret->size());
  return true;
}

}  // namespace

// The SessionManagerClient implementation used in production.
class SessionManagerClientImpl : public SessionManagerClient {
 public:
  SessionManagerClientImpl() = default;

  SessionManagerClientImpl(const SessionManagerClientImpl&) = delete;
  SessionManagerClientImpl& operator=(const SessionManagerClientImpl&) = delete;

  ~SessionManagerClientImpl() override = default;

  // SessionManagerClient overrides:
  void SetStubDelegate(StubDelegate* delegate) override {
    // Do nothing; this isn't a stub implementation.
  }

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  bool HasObserver(const Observer* observer) const override {
    return observers_.HasObserver(observer);
  }

  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override {
    session_manager_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  bool IsScreenLocked() const override { return screen_is_locked_; }

  void EmitLoginPromptVisible() override {
    SimpleMethodCallToSessionManager(
        login_manager::kSessionManagerEmitLoginPromptVisible);
    for (auto& observer : observers_)
      observer.EmitLoginPromptVisibleCalled();
  }

  void EmitAshInitialized() override {
    SimpleMethodCallToSessionManager(
        login_manager::kSessionManagerEmitAshInitialized);
  }

  void RestartJob(int socket_fd,
                  const std::vector<std::string>& argv,
                  RestartJobReason reason,
                  chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerRestartJob);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(socket_fd);
    writer.AppendArrayOfStrings(argv);
    writer.AppendUint32(static_cast<uint32_t>(reason));
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SaveLoginPassword(const std::string& password) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerSaveLoginPassword);
    dbus::MessageWriter writer(&method_call);

    base::ScopedFD fd = CreatePasswordPipe(password);
    if (fd.get() == -1) {
      LOG(WARNING) << "Could not create password pipe.";
      return;
    }

    writer.AppendFileDescriptor(fd.get());

    session_manager_proxy_->CallMethod(&method_call,
                                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                       base::DoNothing());
  }

  void LoginScreenStorageStore(
      const std::string& key,
      const login_manager::LoginScreenStorageMetadata& metadata,
      const std::string& data,
      LoginScreenStorageStoreCallback callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerLoginScreenStorageStore);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(key);

    const std::string metadata_blob = metadata.SerializeAsString();
    writer.AppendArrayOfBytes(base::as_byte_span(metadata_blob));
    writer.AppendUint64(data.size());

    base::ScopedFD fd = CreateSharedMemoryRegionFDWithData(data);
    if (!fd.is_valid()) {
      std::string error = "Could not create shared memory.";
      std::move(callback).Run(std::move(error));
      return;
    }
    writer.AppendFileDescriptor(fd.get());

    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnLoginScreenStorageStore,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void LoginScreenStorageRetrieve(
      const std::string& key,
      LoginScreenStorageRetrieveCallback callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerLoginScreenStorageRetrieve);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(key);
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnLoginScreenStorageRetrieve,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void LoginScreenStorageListKeys(
      LoginScreenStorageListKeysCallback callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerLoginScreenStorageListKeys);
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnLoginScreenStorageListKeys,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void LoginScreenStorageDelete(const std::string& key) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerLoginScreenStorageDelete);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(key);
    session_manager_proxy_->CallMethod(&method_call,
                                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                       base::DoNothing());
  }

  void StartSession(
      const cryptohome::AccountIdentifier& cryptohome_id) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStartSession);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.account_id());
    writer.AppendString("");  // Unique ID is deprecated
    session_manager_proxy_->CallMethod(&method_call,
                                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                       base::DoNothing());
  }

  void StartSessionEx(const cryptohome::AccountIdentifier& cryptohome_id,
                      bool chrome_side_key_generation) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStartSessionEx);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.account_id());
    writer.AppendString("");  // Unique ID is deprecated
    writer.AppendBool(chrome_side_key_generation);
    session_manager_proxy_->CallMethod(&method_call,
                                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                       base::DoNothing());
  }

  void EmitStartedUserSession(
      const cryptohome::AccountIdentifier& cryptohome_id) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerEmitStartedUserSession);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.account_id());
    session_manager_proxy_->CallMethod(&method_call,
                                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                       base::DoNothing());
  }

  void StopSession(login_manager::SessionStopReason reason) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerStopSessionWithReason);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(static_cast<uint32_t>(reason));
    session_manager_proxy_->CallMethod(&method_call,
                                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                       base::DoNothing());
  }

  void LoadShillProfile(
      const cryptohome::AccountIdentifier& cryptohome_id) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerLoadShillProfile);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.account_id());
    session_manager_proxy_->CallMethod(&method_call,
                                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                       base::DoNothing());
  }

  void StartDeviceWipe(chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStartDeviceWipe);
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    for (auto& observer : observers_) {
      observer.PowerwashRequested(/*admin_requested*/ false);
    }
  }

  void StartRemoteDeviceWipe(
      const enterprise_management::SignedData& signed_command) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerStartRemoteDeviceWipe);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(signed_command);
    session_manager_proxy_->CallMethod(&method_call,
                                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                       base::DoNothing());
    for (auto& observer : observers_) {
      observer.PowerwashRequested(/*admin_requested*/ true);
    }
  }

  void ClearForcedReEnrollmentVpd(
      chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerClearForcedReEnrollmentVpd);
    dbus::MessageWriter writer(&method_call);
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UnblockDevModeForEnrollment(
      chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerUnblockDevModeForEnrollment);
    dbus::MessageWriter writer(&method_call);
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UnblockDevModeForCarrierLock(
      chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerUnblockDevModeForCarrierLock);
    dbus::MessageWriter writer(&method_call);
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UnblockDevModeForInitialStateDetermination(
      chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::
            kSessionManagerUnblockDevModeForInitialStateDetermination);
    dbus::MessageWriter writer(&method_call);
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StartTPMFirmwareUpdate(const std::string& update_mode) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerStartTPMFirmwareUpdate);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(update_mode);
    session_manager_proxy_->CallMethod(&method_call,
                                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                       base::DoNothing());
  }

  void RequestLockScreen() override {
    SimpleMethodCallToSessionManager(login_manager::kSessionManagerLockScreen);
  }

  void NotifyLockScreenShown() override {
    SimpleMethodCallToSessionManager(
        login_manager::kSessionManagerHandleLockScreenShown);
  }

  void NotifyLockScreenDismissed() override {
    SimpleMethodCallToSessionManager(
        login_manager::kSessionManagerHandleLockScreenDismissed);
  }

  bool BlockingRequestBrowserDataMigration(
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& mode) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerStartBrowserDataMigration);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.account_id());
    writer.AppendString(mode);
    auto result = blocking_method_caller_->CallMethodAndBlock(&method_call);
    if (!result.has_value()) {
      LOG(ERROR) << "BlockingRequestBrowserDataMigration failed :"
                 << result.error().name() << ":" << result.error().message();
      return false;
    }

    return true;
  }

  bool BlockingRequestBrowserDataBackwardMigration(
      const cryptohome::AccountIdentifier& cryptohome_id) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerStartBrowserDataBackwardMigration);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.account_id());
    auto result = blocking_method_caller_->CallMethodAndBlock(&method_call);
    if (!result.has_value()) {
      LOG(ERROR) << "BlockingRequestBrowserDataBackwardMigration failed :"
                 << result.error().name() << ":" << result.error().message();
      return false;
    }

    return true;
  }

  void RetrieveActiveSessions(ActiveSessionsCallback callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerRetrieveActiveSessions);

    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnRetrieveActiveSessions,
                       weak_ptr_factory_.GetWeakPtr(),
                       login_manager::kSessionManagerRetrieveActiveSessions,
                       std::move(callback)));
  }

  void RetrievePolicy(const login_manager::PolicyDescriptor& descriptor,
                      RetrievePolicyCallback callback) override {
    CallRetrievePolicy(descriptor, std::move(callback));
  }

  RetrievePolicyResponseType BlockingRetrievePolicy(
      const login_manager::PolicyDescriptor& descriptor,
      std::string* policy_out) override {
    return CallBlockingRetrievePolicy(descriptor, policy_out);
  }

  void StoreDevicePolicy(const std::string& policy_blob,
                         chromeos::VoidDBusMethodCallback callback) override {
    login_manager::PolicyDescriptor descriptor =
        ash::MakeChromePolicyDescriptor(login_manager::ACCOUNT_TYPE_DEVICE,
                                        kEmptyAccountId);
    CallStorePolicy(descriptor, policy_blob, std::move(callback));
  }

  void StorePolicyForUser(const cryptohome::AccountIdentifier& cryptohome_id,
                          const std::string& policy_blob,
                          chromeos::VoidDBusMethodCallback callback) override {
    login_manager::PolicyDescriptor descriptor =
        ash::MakeChromePolicyDescriptor(login_manager::ACCOUNT_TYPE_USER,
                                        cryptohome_id.account_id());
    CallStorePolicy(descriptor, policy_blob, std::move(callback));
  }

  void StoreDeviceLocalAccountPolicy(
      const std::string& account_name,
      const std::string& policy_blob,
      chromeos::VoidDBusMethodCallback callback) override {
    login_manager::PolicyDescriptor descriptor =
        ash::MakeChromePolicyDescriptor(
            login_manager::ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT, account_name);
    CallStorePolicy(descriptor, policy_blob, std::move(callback));
  }

  void StorePolicy(const login_manager::PolicyDescriptor& descriptor,
                   const std::string& policy_blob,
                   chromeos::VoidDBusMethodCallback callback) override {
    CallStorePolicy(descriptor, policy_blob, std::move(callback));
  }

  bool SupportsBrowserRestart() const override { return true; }

  void SetFlagsForUser(const cryptohome::AccountIdentifier& cryptohome_id,
                       const std::vector<std::string>& flags) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerSetFlagsForUser);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.account_id());
    writer.AppendArrayOfStrings(flags);
    session_manager_proxy_->CallMethod(&method_call,
                                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                       base::DoNothing());
  }

  void SetFeatureFlagsForUser(
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::vector<std::string>& feature_flags,
      const std::map<std::string, std::string>& origin_list_flags) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerSetFeatureFlagsForUser);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.account_id());
    writer.AppendArrayOfStrings(feature_flags);

    dbus::MessageWriter dict_writer(nullptr);
    writer.OpenArray("{ss}", &dict_writer);
    for (const auto& origin_entry : origin_list_flags) {
      dbus::MessageWriter entry_writer(nullptr);
      dict_writer.OpenDictEntry(&entry_writer);
      entry_writer.AppendString(origin_entry.first);
      entry_writer.AppendString(origin_entry.second);
      dict_writer.CloseContainer(&entry_writer);
    }
    writer.CloseContainer(&dict_writer);

    session_manager_proxy_->CallMethod(&method_call,
                                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                       base::DoNothing());
  }

  void GetServerBackedStateKeys(StateKeysCallback callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerGetServerBackedStateKeys);

    // Infinite timeout needed because the state keys are not generated as long
    // as the time sync hasn't been done (which requires network).
    // TODO(igorcov): Since this is a resource allocated that could last a long
    // time, we will need to change the behavior to either listen to
    // LastSyncInfo event from tlsdated or communicate through signals with
    // session manager in this particular flow.
    session_manager_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_INFINITE,
        base::BindOnce(&SessionManagerClientImpl::OnGetServerBackedStateKeys,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetPsmDeviceActiveSecret(
      PsmDeviceActiveSecretCallback callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerGetPsmDeviceActiveSecret);

    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnGetPsmDeviceActiveSecret,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StartArcMiniContainer(
      const arc::StartArcMiniInstanceRequest& request,
      chromeos::VoidDBusMethodCallback callback) override {
    DCHECK(!callback.is_null());
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerStartArcMiniContainer);
    dbus::MessageWriter writer(&method_call);

    writer.AppendProtoAsArrayOfBytes(request);

    session_manager_proxy_->CallMethod(
        &method_call, kStartArcTimeout,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UpgradeArcContainer(const arc::UpgradeArcContainerRequest& request,
                           chromeos::VoidDBusMethodCallback callback) override {
    DCHECK(!callback.is_null());
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerUpgradeArcContainer);
    dbus::MessageWriter writer(&method_call);

    writer.AppendProtoAsArrayOfBytes(request);

    session_manager_proxy_->CallMethod(
        &method_call, kUpgradeTimeoutMs,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StopArcInstance(const std::string& account_id,
                       bool should_backup_log,
                       chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStopArcInstance);

    dbus::MessageWriter writer(&method_call);
    writer.AppendString(account_id);
    writer.AppendBool(should_backup_log);

    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetArcCpuRestriction(
      login_manager::ContainerCpuRestrictionState restriction_state,
      chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerSetArcCpuRestriction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(restriction_state);
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void EmitArcBooted(const cryptohome::AccountIdentifier& cryptohome_id,
                     chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerEmitArcBooted);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(cryptohome_id.account_id());
    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetArcStartTime(
      chromeos::DBusMethodCallback<base::TimeTicks> callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerGetArcStartTimeTicks);

    session_manager_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnGetArcStartTime,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void EnableAdbSideload(EnableAdbSideloadCallback callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerEnableAdbSideload);

    session_manager_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnEnableAdbSideload,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void QueryAdbSideload(QueryAdbSideloadCallback callback) override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerQueryAdbSideload);

    session_manager_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnQueryAdbSideload,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void Init(dbus::Bus* bus) {
    session_manager_proxy_ = bus->GetObjectProxy(
        login_manager::kSessionManagerServiceName,
        dbus::ObjectPath(login_manager::kSessionManagerServicePath));
    blocking_method_caller_ = std::make_unique<chromeos::BlockingMethodCaller>(
        bus, session_manager_proxy_);

    // Signals emitted on the session manager's interface.
    session_manager_proxy_->ConnectToSignal(
        login_manager::kSessionManagerInterface,
        login_manager::kOwnerKeySetSignal,
        base::BindRepeating(&SessionManagerClientImpl::OwnerKeySetReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&SessionManagerClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    session_manager_proxy_->ConnectToSignal(
        login_manager::kSessionManagerInterface,
        login_manager::kPropertyChangeCompleteSignal,
        base::BindRepeating(
            &SessionManagerClientImpl::PropertyChangeCompleteReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&SessionManagerClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    session_manager_proxy_->ConnectToSignal(
        login_manager::kSessionManagerInterface,
        login_manager::kScreenIsLockedSignal,
        base::BindRepeating(&SessionManagerClientImpl::ScreenIsLockedReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&SessionManagerClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    session_manager_proxy_->ConnectToSignal(
        login_manager::kSessionManagerInterface,
        login_manager::kScreenIsUnlockedSignal,
        base::BindRepeating(&SessionManagerClientImpl::ScreenIsUnlockedReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&SessionManagerClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    session_manager_proxy_->ConnectToSignal(
        login_manager::kSessionManagerInterface,
        login_manager::kArcInstanceStopped,
        base::BindRepeating(
            &SessionManagerClientImpl::ArcInstanceStoppedReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&SessionManagerClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    session_manager_proxy_->ConnectToSignal(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionStateChangedSignal,
        base::BindRepeating(&SessionManagerClientImpl::SessionStateChanged,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&SessionManagerClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Makes a method call to the session manager with no arguments and no
  // response.
  void SimpleMethodCallToSessionManager(const std::string& method_name) {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 method_name);
    session_manager_proxy_->CallMethod(&method_call,
                                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                       base::DoNothing());
  }

  // Called when the method call without result is completed.
  void OnVoidMethod(chromeos::VoidDBusMethodCallback callback,
                    dbus::Response* response) {
    std::move(callback).Run(response);
  }

  // Non-blocking call to Session Manager to retrieve policy.
  void CallRetrievePolicy(const login_manager::PolicyDescriptor& descriptor,
                          RetrievePolicyCallback callback) {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerRetrievePolicyEx);
    dbus::MessageWriter writer(&method_call);
    const std::string descriptor_blob = descriptor.SerializeAsString();
    // static_cast does not work due to signedness.
    writer.AppendArrayOfBytes(base::as_byte_span(descriptor_blob));
    session_manager_proxy_->CallMethodWithErrorResponse(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SessionManagerClientImpl::OnRetrievePolicy,
                       weak_ptr_factory_.GetWeakPtr(),
                       descriptor.account_type(), std::move(callback)));
  }

  // Blocking call to Session Manager to retrieve policy.
  RetrievePolicyResponseType CallBlockingRetrievePolicy(
      const login_manager::PolicyDescriptor& descriptor,
      std::string* policy_out) {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerRetrievePolicyEx);
    dbus::MessageWriter writer(&method_call);
    const std::string descriptor_blob = descriptor.SerializeAsString();
    // static_cast does not work due to signedness.
    writer.AppendArrayOfBytes(base::as_byte_span(descriptor_blob));
    auto result = blocking_method_caller_->CallMethodAndBlock(&method_call);
    RetrievePolicyResponseType response_type =
        RetrievePolicyResponseType::SUCCESS;
    if (!result.has_value() && result.error().IsValid()) {
      response_type = GetPolicyResponseTypeByError(result.error().name());
    }
    if (response_type == RetrievePolicyResponseType::SUCCESS) {
      ExtractPolicyResponseString(descriptor.account_type(),
                                  result.has_value() ? result->get() : nullptr,
                                  policy_out);
    } else {
      policy_out->clear();
    }
    return response_type;
  }

  void CallStorePolicy(const login_manager::PolicyDescriptor& descriptor,
                       const std::string& policy_blob,
                       chromeos::VoidDBusMethodCallback callback) {
    dbus::MethodCall method_call(login_manager::kSessionManagerInterface,
                                 login_manager::kSessionManagerStorePolicyEx);
    dbus::MessageWriter writer(&method_call);
    const std::string descriptor_blob = descriptor.SerializeAsString();
    // static_cast does not work due to signedness.
    writer.AppendArrayOfBytes(base::as_byte_span(descriptor_blob));
    writer.AppendArrayOfBytes(base::as_byte_span(policy_blob));
    // The timeout is intentionally chosen to be that big because on some
    // devices the operation is slow and a short timeout would lead to
    // unnecessary enrollment failures. See crbug.com/1155533 for context.
    session_manager_proxy_->CallMethod(
        &method_call, /*timeout_ms=*/180000,
        base::BindOnce(&SessionManagerClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Called when kSessionManagerRetrieveActiveSessions method is complete.
  void OnRetrieveActiveSessions(const std::string& method_name,
                                ActiveSessionsCallback callback,
                                dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    dbus::MessageReader reader(response);
    dbus::MessageReader array_reader(nullptr);
    if (!reader.PopArray(&array_reader)) {
      LOG(ERROR) << method_name
                 << " response is incorrect: " << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }

    ActiveSessionsMap sessions;
    while (array_reader.HasMoreData()) {
      dbus::MessageReader dict_entry_reader(nullptr);
      std::string key;
      std::string value;
      if (!array_reader.PopDictEntry(&dict_entry_reader) ||
          !dict_entry_reader.PopString(&key) ||
          !dict_entry_reader.PopString(&value)) {
        LOG(ERROR) << method_name
                   << " response is incorrect: " << response->ToString();
      } else {
        sessions[key] = value;
      }
    }
    std::move(callback).Run(std::move(sessions));
  }

  void OnLoginScreenStorageStore(LoginScreenStorageStoreCallback callback,
                                 dbus::Response* response) {
    std::move(callback).Run(std::nullopt);
  }

  void OnLoginScreenStorageRetrieve(LoginScreenStorageRetrieveCallback callback,
                                    dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::nullopt /* data */,
                              "LoginScreenStorageRetrieve() D-Bus method "
                              "returned an empty response");
      return;
    }

    dbus::MessageReader reader(response);
    base::ScopedFD result_fd;
    uint64_t result_size;
    if (!reader.PopUint64(&result_size) ||
        !reader.PopFileDescriptor(&result_fd)) {
      std::string error = "Invalid response: " + response->ToString();
      std::move(callback).Run(std::nullopt /* data */, error);
      return;
    }
    std::vector<uint8_t> result_data;
    if (!ReadSecretFromSharedMemory(std::move(result_fd), result_size,
                                    &result_data)) {
      std::string error = "Couldn't read retrieved data from shared memory.";
      std::move(callback).Run(std::nullopt /* data */, error);
      return;
    }
    std::move(callback).Run(std::string(result_data.begin(), result_data.end()),
                            std::nullopt /* error */);
  }

  void OnLoginScreenStorageListKeys(LoginScreenStorageListKeysCallback callback,
                                    dbus::Response* response) {
    if (!response) {
      // TODO(voit): Add more granular error handling: key is not found error vs
      // general 'something went wrong' error.
      std::move(callback).Run({} /* keys */,
                              "LoginScreenStorageListKeys() D-Bus method "
                              "returned an empty response");
      return;
    }
    dbus::MessageReader reader(response);
    std::vector<std::string> keys;
    if (!reader.PopArrayOfStrings(&keys)) {
      std::string error = "Invalid response: " + response->ToString();
      std::move(callback).Run({} /* keys */, error);
      return;
    }
    std::move(callback).Run(std::move(keys), std::nullopt /* error */);
  }

  // Reads an array of policy data bytes data as std::string.
  void ExtractPolicyResponseString(
      login_manager::PolicyAccountType account_type,
      dbus::Response* response,
      std::string* extracted) {
    if (!response) {
      LOG(ERROR) << "Failed to call RetrievePolicyEx for account type "
                 << account_type;
      return;
    }
    dbus::MessageReader reader(response);
    const uint8_t* values = nullptr;
    size_t length = 0;
    if (!reader.PopArrayOfBytes(&values, &length)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      return;
    }
    // static_cast does not work due to signedness.
    extracted->assign(reinterpret_cast<const char*>(values), length);
  }

  // Called when kSessionManagerRetrievePolicy or
  // kSessionManagerRetrievePolicyForUser method is complete.
  void OnRetrievePolicy(login_manager::PolicyAccountType account_type,
                        RetrievePolicyCallback callback,
                        dbus::Response* response,
                        dbus::ErrorResponse* error) {
    if (!response) {
      RetrievePolicyResponseType response_type =
          GetPolicyResponseTypeByError(error ? error->GetErrorName() : "");
      std::move(callback).Run(response_type, std::string());
      return;
    }

    dbus::MessageReader reader(response);
    std::string proto_blob;
    ExtractPolicyResponseString(account_type, response, &proto_blob);
    std::move(callback).Run(RetrievePolicyResponseType::SUCCESS, proto_blob);
  }

  // Called when the owner key set signal is received.
  void OwnerKeySetReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string result_string;
    if (!reader.PopString(&result_string)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    const bool success = base::StartsWith(result_string, "success",
                                          base::CompareCase::INSENSITIVE_ASCII);
    for (auto& observer : observers_)
      observer.OwnerKeySet(success);
  }

  // Called when the property change complete signal is received.
  void PropertyChangeCompleteReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string result_string;
    if (!reader.PopString(&result_string)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    const bool success = base::StartsWith(result_string, "success",
                                          base::CompareCase::INSENSITIVE_ASCII);
    for (auto& observer : observers_)
      observer.PropertyChangeComplete(success);
  }

  void ScreenIsLockedReceived(dbus::Signal* signal) {
    screen_is_locked_ = true;
    for (auto& observer : observers_)
      observer.ScreenLockedStateUpdated();
  }

  void ScreenIsUnlockedReceived(dbus::Signal* signal) {
    screen_is_locked_ = false;
    for (auto& observer : observers_)
      observer.ScreenLockedStateUpdated();
  }

  void ArcInstanceStoppedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    uint32_t reason = 0;
    if (!reader.PopUint32(&reason)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    for (auto& observer : observers_) {
      observer.ArcInstanceStopped(
          static_cast<login_manager::ArcContainerStopReason>(reason));
    }
  }

  void SessionStateChanged(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string result_string;
    if (!reader.PopString(&result_string)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    if (result_string == kStopping) {
      for (auto& observer : observers_) {
        observer.SessionStopping();
      }
    }
  }

  // Called when the object is connected to the signal.
  void SignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
    LOG_IF(ERROR, !success) << "Failed to connect to " << signal_name;
  }

  // Called when kSessionManagerGetServerBackedStateKeys method is complete.
  void OnGetServerBackedStateKeys(StateKeysCallback callback,
                                  dbus::Response* response,
                                  dbus::ErrorResponse* error_response) {
    if (error_response &&
        error_response->GetErrorName() ==
            login_manager::dbus_error::kStateKeysRequestFail) {
      // Session manager failed to generate state keys, report identifiers
      // error.
      return std::move(callback).Run(
          base::unexpected(StateKeyErrorType::kMissingIdentifiers));
    }

    if (!response) {
      // DBus call failed with unspecified error, report communication error.
      return std::move(callback).Run(
          base::unexpected(StateKeyErrorType::kCommunicationError));
    }

    dbus::MessageReader reader(response);
    dbus::MessageReader array_reader(nullptr);
    if (!reader.PopArray(&array_reader)) {
      LOG(ERROR) << "Bad response (not an array of keys): "
                 << response->ToString();
      std::move(callback).Run(
          base::unexpected(StateKeyErrorType::kInvalidResponse));
      return;
    }

    std::vector<std::string> state_keys;
    while (array_reader.HasMoreData()) {
      const uint8_t* data = nullptr;
      size_t size = 0;
      if (!array_reader.PopArrayOfBytes(&data, &size)) {
        LOG(ERROR) << "Bad response (not an array of bytes): "
                   << response->ToString();
        std::move(callback).Run(
            base::unexpected(StateKeyErrorType::kInvalidResponse));
        return;
      }
      if (size == 0) {
        LOG(ERROR) << "Bad response (empty array of bytes): "
                   << response->ToString();
        std::move(callback).Run(
            base::unexpected(StateKeyErrorType::kInvalidResponse));
        return;
      }
      state_keys.emplace_back(reinterpret_cast<const char*>(data), size);
    }

    if (state_keys.empty()) {
      // TODO(b/318708647): Session manager did not report an error but still
      // responded with empty state keys. Report something else than missing
      // identifiers.
      std::move(callback).Run(
          base::unexpected(StateKeyErrorType::kMissingIdentifiers));
      return;
    }
    std::move(callback).Run(base::ok(std::move(state_keys)));
  }

  // Called when kSessionManagerGetPsmDeviceActiveSecret method is complete.
  void OnGetPsmDeviceActiveSecret(PsmDeviceActiveSecretCallback callback,
                                  dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to get response OnGetPsmDeviceActiveSecret.";
      std::move(callback).Run(std::string());
      return;
    }

    std::string psm_device_active_secret;
    dbus::MessageReader reader(response);

    if (!reader.PopString(&psm_device_active_secret)) {
      LOG(ERROR) << "Received a non-string response from dbus.";
      std::move(callback).Run(std::string());
      return;
    }

    std::move(callback).Run(psm_device_active_secret);
  }

  void OnGetArcStartTime(chromeos::DBusMethodCallback<base::TimeTicks> callback,
                         dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    dbus::MessageReader reader(response);
    int64_t ticks = 0;
    if (!reader.PopInt64(&ticks)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(base::TimeTicks::FromInternalValue(ticks));
  }

  void OnEnableAdbSideload(EnableAdbSideloadCallback callback,
                           dbus::Response* response,
                           dbus::ErrorResponse* error) {
    if (!response) {
      LOG(ERROR) << "Failed to call EnableAdbSideload: "
                 << (error ? error->ToString() : "(null)");
      if (error && error->GetErrorName() == DBUS_ERROR_NOT_SUPPORTED) {
        std::move(callback).Run(AdbSideloadResponseCode::NEED_POWERWASH);
      } else {
        std::move(callback).Run(AdbSideloadResponseCode::FAILED);
      }
      return;
    }

    bool succeeded;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&succeeded)) {
      LOG(ERROR) << "Failed to enable sideloading";
      std::move(callback).Run(AdbSideloadResponseCode::FAILED);
      return;
    }
    std::move(callback).Run(AdbSideloadResponseCode::SUCCESS);
  }

  void OnQueryAdbSideload(QueryAdbSideloadCallback callback,
                          dbus::Response* response,
                          dbus::ErrorResponse* error) {
    if (!response) {
      LOG(ERROR) << "Failed to call QueryAdbSideload: "
                 << (error ? error->ToString() : "(null)");
      if (error && error->GetErrorName() == DBUS_ERROR_NOT_SUPPORTED) {
        std::move(callback).Run(AdbSideloadResponseCode::NEED_POWERWASH, false);
      } else {
        std::move(callback).Run(AdbSideloadResponseCode::FAILED, false);
      }
      return;
    }

    bool is_allowed;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&is_allowed)) {
      LOG(ERROR) << "Failed to interpret the response";
      std::move(callback).Run(AdbSideloadResponseCode::FAILED, false);
      return;
    }
    std::move(callback).Run(AdbSideloadResponseCode::SUCCESS, is_allowed);
  }

  raw_ptr<dbus::ObjectProxy> session_manager_proxy_ = nullptr;
  std::unique_ptr<chromeos::BlockingMethodCaller> blocking_method_caller_;
  base::ObserverList<Observer>::UncheckedAndDanglingUntriaged observers_{
      SessionManagerClient::kObserverListPolicy};

  // Most recent screen-lock state received from session_manager.
  bool screen_is_locked_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SessionManagerClientImpl> weak_ptr_factory_{this};
};

SessionManagerClient::SessionManagerClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

SessionManagerClient::~SessionManagerClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void SessionManagerClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new SessionManagerClientImpl)->Init(bus);
}

// static
void SessionManagerClient::InitializeFake() {
  // Do not create a new fake if it was initialized early in a browser test (for
  // early setup calls dependent on SessionManagerClient).
  if (!FakeSessionManagerClient::Get()) {
    new FakeSessionManagerClient(
        FakeSessionManagerClient::PolicyStorageType::kOnDisk);
  }
}

// static
void SessionManagerClient::InitializeFakeInMemory() {
  if (!FakeSessionManagerClient::Get()) {
    new FakeSessionManagerClient(
        FakeSessionManagerClient::PolicyStorageType::kInMemory);
  }
}

// static
void SessionManagerClient::Shutdown() {
  // Note `g_instance` could be nullptr when ScopedFakeSessionManagerClient is
  // used.
  delete g_instance;
}

// static
SessionManagerClient* SessionManagerClient::Get() {
  return g_instance;
}

}  // namespace ash
