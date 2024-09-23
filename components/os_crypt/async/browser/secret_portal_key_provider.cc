// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/secret_portal_key_provider.h"

#include <fcntl.h>
#include <linux/limits.h>

#include <array>
#include <utility>

#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/current_thread.h"
#include "components/dbus/properties/types.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/name_has_owner.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "crypto/hkdf.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace os_crypt_async {

namespace {

constexpr char kSaltForHkdf[] = "fdo_portal_secret_salt";
constexpr char kInfoForHkdf[] = "HKDF-SHA-256 AES-256-GCM";

scoped_refptr<dbus::Bus> CreateBus() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SESSION;
  options.connection_type = dbus::Bus::PRIVATE;
  options.dbus_task_runner = dbus_thread_linux::GetTaskRunner();
  return base::MakeRefCounted<dbus::Bus>(options);
}

}  // namespace

// static
void SecretPortalKeyProvider::RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kOsCryptTokenPrefName, "");
  registry->RegisterStringPref(kOsCryptPrevDesktopPrefName, "");
  registry->RegisterBooleanPref(kOsCryptPrevInitSuccessPrefName, false);
}

SecretPortalKeyProvider::SecretPortalKeyProvider(PrefService* local_state,
                                                 bool use_for_encryption)
    : SecretPortalKeyProvider(local_state, CreateBus(), use_for_encryption) {}

SecretPortalKeyProvider::~SecretPortalKeyProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bus_->GetDBusTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&dbus::Bus::ShutdownAndBlock, bus_));
}

SecretPortalKeyProvider::SecretPortalKeyProvider(PrefService* local_state,
                                                 scoped_refptr<dbus::Bus> bus,
                                                 bool use_for_encryption)
    : local_state_(local_state),
      use_for_encryption_(use_for_encryption),
      bus_(bus) {}

void SecretPortalKeyProvider::GetKey(KeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!key_callback_);
  CHECK(callback);
  key_callback_ = std::move(callback);

  dbus_utils::NameHasOwner(
      bus_.get(), GetSecretServiceName(),
      base::BindOnce(&SecretPortalKeyProvider::OnNameHasOwnerResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool SecretPortalKeyProvider::UseForEncryption() {
  return use_for_encryption_;
}

bool SecretPortalKeyProvider::IsCompatibleWithOsCryptSync() {
  return false;
}

void SecretPortalKeyProvider::OnNameHasOwnerResponse(
    std::optional<bool> name_has_owner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!name_has_owner.value_or(false)) {
    return Finalize(InitStatus::kNoService);
  }

  // Create a pipe to retrieve the secret.
  int fds[2];
  if (pipe2(fds, O_CLOEXEC) != 0) {
    LOG(ERROR) << "Failed to create pipe for secret retrieval.";
    return Finalize(InitStatus::kPipeFailed);
  }
  read_fd_ = base::ScopedFD(fds[0]);
  base::ScopedFD write_fd(fds[1]);

  dbus::MethodCall method_call(kInterfaceSecret, kMethodRetrieveSecret);

  response_path_ =
      std::make_unique<dbus::ObjectPath>(base::nix::XdgDesktopPortalRequestPath(
          bus_->GetConnectionName(), kHandleToken));

  auto* response_proxy =
      bus_->GetObjectProxy(GetSecretServiceName(), *response_path_);
  response_proxy->ConnectToSignal(
      kInterfaceRequest, kSignalResponse,
      base::BindRepeating(&SecretPortalKeyProvider::OnResponseSignal,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SecretPortalKeyProvider::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));

  dbus::MessageWriter writer(&method_call);
  writer.AppendFileDescriptor(write_fd.get() /* the FD gets duplicated */);
  DbusDictionary options;
  if (local_state_->HasPrefPath(kOsCryptTokenPrefName)) {
    const std::string token = local_state_->GetString(kOsCryptTokenPrefName);
    if (!token.empty()) {
      options.Put("token", MakeDbusVariant(DbusString(token)));
    }
  }
  options.Put("handle_token", MakeDbusVariant(DbusString(kHandleToken)));
  options.Write(&writer);

  auto* secret_proxy = bus_->GetObjectProxy(
      GetSecretServiceName(), dbus::ObjectPath(kObjectPathSecret));
  secret_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&SecretPortalKeyProvider::OnRetrieveSecretResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SecretPortalKeyProvider::OnRetrieveSecretResponse(
    dbus::Response* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!response) {
    LOG(ERROR) << "Failed to retrieve secret: No response from portal.";
    return Finalize(InitStatus::kNoResponse);
  }

  dbus::MessageReader reader(response);

  // Read the object path of the response handle.
  dbus::ObjectPath response_path;
  if (!reader.PopObjectPath(&response_path)) {
    LOG(ERROR) << "Failed to retrieve secret: Invalid response format.";
    return Finalize(InitStatus::kInvalidResponseFormat);
  }
  CHECK(response_path_);
  const bool matches = response_path == *response_path_;
  response_path_.reset();
  if (!matches) {
    LOG(ERROR) << "Response path does not match.";
    return Finalize(InitStatus::kResponsePathMismatch);
  }

  // Read the secret from the pipe.  This must happen asynchronously because the
  // file may not become readable until the keyring is unlocked by typing a
  // password.
  read_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      read_fd_.get(),
      base::BindRepeating(&SecretPortalKeyProvider::OnFdReadable,
                          weak_ptr_factory_.GetWeakPtr()));
}

void SecretPortalKeyProvider::OnSignalConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool connected) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connected) {
    LOG(ERROR) << "Failed to connect to " << interface_name << "."
               << signal_name;
    return Finalize(InitStatus::kSignalConnectFailed);
  }
}

void SecretPortalKeyProvider::OnResponseSignal(dbus::Signal* signal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dbus::MessageReader reader(signal);
  uint32_t response;
  if (!reader.PopUint32(&response)) {
    LOG(ERROR) << "Failed to read response from signal.";
    return Finalize(InitStatus::kSignalReadFailed);
  }
  if (response != 0) {
    LOG(ERROR) << "Keyring unlock cancelled: " << response;
    return Finalize(InitStatus::kUserCancelledUnlock);
  }
  dbus::MessageReader dict_reader(nullptr);
  if (!reader.PopArray(&dict_reader)) {
    LOG(ERROR) << "Failed to read array.";
    return Finalize(InitStatus::kSignalParseFailed);
  }

  bool got_token = false;
  while (dict_reader.HasMoreData()) {
    dbus::MessageReader dict_entry_reader(nullptr);
    if (!dict_reader.PopDictEntry(&dict_entry_reader)) {
      LOG(ERROR) << "Failed to read dict entry.";
      return Finalize(InitStatus::kSignalParseFailed);
    }
    std::string key;
    if (!dict_entry_reader.PopString(&key)) {
      LOG(ERROR) << "Failed to read key.";
      return Finalize(InitStatus::kSignalParseFailed);
    }
    if (key == "token") {
      std::string value;
      if (!dict_entry_reader.PopVariantOfString(&value)) {
        LOG(ERROR) << "Failed to read value.";
        return Finalize(InitStatus::kSignalParseFailed);
      }
      local_state_->SetString(kOsCryptTokenPrefName, value);
      got_token = true;
    }
  }

  // TODO(https://crbug.com/40086962): Investigate if a token should be
  // required to continue.
  base::UmaHistogramBoolean(kUmaGotTokenBoolean, got_token);
}

void SecretPortalKeyProvider::OnFdReadable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::array<uint8_t, PIPE_BUF> buffer;
  ssize_t bytes_read = read(read_fd_.get(), buffer.data(), buffer.size());
  if (bytes_read < 0) {
    LOG(ERROR) << "Failed to read secret from file descriptor.";
    return Finalize(InitStatus::kPipeReadFailed);
  }
  if (bytes_read > 0) {
    auto buffer_span =
        base::span(buffer).subspan(0u, base::checked_cast<size_t>(bytes_read));
    secret_.insert(secret_.end(), buffer_span.begin(), buffer_span.end());
    return;
  }

  // EOF.
  read_watcher_.reset();
  read_fd_.reset();
  ReceivedSecret();
}

void SecretPortalKeyProvider::ReceivedSecret() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (secret_.empty()) {
    LOG(ERROR) << "Retrieved secret is empty.";
    return Finalize(InitStatus::kEmptySecret);
  }

  auto hashed = crypto::HkdfSha256(
      base::span(secret_), base::as_byte_span(kSaltForHkdf),
      base::as_byte_span(kInfoForHkdf), Encryptor::Key::kAES256GCMKeySize);
  secret_.clear();

  Encryptor::Key derived_key(hashed, mojom::Algorithm::kAES256GCM);
  Finalize(InitStatus::kSuccess, kKeyTag, std::move(derived_key));
}

void SecretPortalKeyProvider::Finalize(InitStatus init_status) {
  CHECK_NE(init_status, InitStatus::kSuccess);
  Finalize(init_status, std::string(), std::nullopt);
}

void SecretPortalKeyProvider::Finalize(InitStatus init_status,
                                       const std::string& tag,
                                       std::optional<Encryptor::Key> key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(init_status == InitStatus::kSuccess, key.has_value());
  if (!key_callback_) {
    // Already finalized.
    return;
  }

  std::move(key_callback_).Run(tag, std::move(key));

  response_path_.reset();
  read_watcher_.reset();
  read_fd_.reset();
  secret_.clear();

  base::UmaHistogramEnumeration(kUmaInitStatusEnum, init_status);

  std::string desktop;
  base::Environment::Create()->GetVar(base::nix::kXdgCurrentDesktopEnvVar,
                                      &desktop);

  const bool success = init_status == InitStatus::kSuccess;

  if (local_state_->HasPrefPath(kOsCryptPrevDesktopPrefName)) {
    const std::string prev_desktop =
        local_state_->GetString(kOsCryptPrevDesktopPrefName);
    if (desktop == prev_desktop) {
      bool prev_init_success =
          local_state_->GetBoolean(kOsCryptPrevInitSuccessPrefName);
      if (prev_init_success && !success) {
        base::UmaHistogramEnumeration(kUmaNewInitFailureEnum, init_status);
      }
    }
  }

  local_state_->SetString(kOsCryptPrevDesktopPrefName, desktop);
  local_state_->SetBoolean(kOsCryptPrevInitSuccessPrefName, success);
}

// static
std::string SecretPortalKeyProvider::GetSecretServiceName() {
  return GetSecretServiceNameForTest().value_or(kServiceSecret);
}

// static
std::optional<std::string>&
SecretPortalKeyProvider::GetSecretServiceNameForTest() {
  static base::NoDestructor<std::optional<std::string>>
      secret_service_name_for_test;
  return *secret_service_name_for_test;
}

}  // namespace os_crypt_async
