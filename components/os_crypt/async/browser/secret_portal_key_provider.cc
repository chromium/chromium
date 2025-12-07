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
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/name_has_owner.h"
#include "components/dbus/xdg/portal.h"
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

}  // namespace

// static
void SecretPortalKeyProvider::RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kOsCryptTokenPrefName, "");
  registry->RegisterStringPref(kOsCryptPrevDesktopPrefName, "");
  registry->RegisterBooleanPref(kOsCryptPrevInitSuccessPrefName, false);
}

SecretPortalKeyProvider::SecretPortalKeyProvider(PrefService* local_state,
                                                 bool use_for_encryption)
    : SecretPortalKeyProvider(local_state,
                              dbus_thread_linux::GetSharedSessionBus(),
                              use_for_encryption) {}

SecretPortalKeyProvider::~SecretPortalKeyProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

  dbus_xdg::RequestXdgDesktopPortal(
      bus_.get(),
      base::BindOnce(&SecretPortalKeyProvider::OnPortalServiceStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool SecretPortalKeyProvider::UseForEncryption() {
  return use_for_encryption_;
}

bool SecretPortalKeyProvider::IsCompatibleWithOsCryptSync() {
  return false;
}

void SecretPortalKeyProvider::OnPortalServiceStarted(bool service_started) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!service_started) {
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

  dbus_xdg::Dictionary options;
  if (local_state_->HasPrefPath(kOsCryptTokenPrefName)) {
    const std::string token = local_state_->GetString(kOsCryptTokenPrefName);
    if (!token.empty()) {
      options["token"] = dbus_utils::Variant::Wrap<"s">(token);
    }
  }

  auto* secret_proxy = bus_->GetObjectProxy(
      GetSecretServiceName(), dbus::ObjectPath(kObjectPathSecret));
  request_ = dbus_xdg::Request::CreateWithPortalServiceName(
      bus_, secret_proxy, kInterfaceSecret, kMethodRetrieveSecret,
      std::move(options),
      base::BindOnce(&SecretPortalKeyProvider::OnRetrieveSecret,
                     weak_ptr_factory_.GetWeakPtr()),
      GetSecretServiceName(), std::move(write_fd));
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

void SecretPortalKeyProvider::OnRetrieveSecret(
    base::expected<dbus_xdg::Dictionary, dbus_xdg::ResponseError> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!results.has_value()) {
    switch (results.error()) {
      case dbus_xdg::ResponseError::kMethodCallFailed:
        return Finalize(InitStatus::kNoResponse);
      case dbus_xdg::ResponseError::kSignalConnectionFailed:
        return Finalize(InitStatus::kSignalConnectFailed);
      case dbus_xdg::ResponseError::kInvalidMethodResponse:
        return Finalize(InitStatus::kInvalidResponseFormat);
      case dbus_xdg::ResponseError::kInvalidSignalResponse:
        return Finalize(InitStatus::kSignalReadFailed);
      case dbus_xdg::ResponseError::kRequestCancelledByUser:
        return Finalize(InitStatus::kUserCancelledUnlock);
      case dbus_xdg::ResponseError::kRequestCancelledOther:
        return Finalize(InitStatus::kOtherCancelledUnlock);
      case dbus_xdg::ResponseError::kInvalidResponseCode:
        return Finalize(InitStatus::kInvalidResponseCode);
    }
  }

  // Read the secret from the pipe.  This must happen asynchronously because the
  // file may not become readable until the keyring is unlocked by typing a
  // password.
  read_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      read_fd_.get(),
      base::BindRepeating(&SecretPortalKeyProvider::OnFdReadable,
                          weak_ptr_factory_.GetWeakPtr()));

  // Though it is documented in the spec, xdg-desktop-portal does not currently
  // implement returning a token.
  dbus_xdg::Dictionary result_dict = std::move(*results);
  std::optional<std::string> token;
  if (auto it = result_dict.find("token"); it != result_dict.end()) {
    token = std::move(it->second).Take<std::string>();
  }
  if (token) {
    local_state_->SetString(kOsCryptTokenPrefName, *token);
  }
  base::UmaHistogramBoolean(kUmaGotTokenBoolean, token.has_value());
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
        base::span(buffer).first(base::checked_cast<size_t>(bytes_read));
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

  auto hashed = crypto::HkdfSha256<Encryptor::Key::kAES256GCMKeySize>(
      base::span(secret_), base::as_byte_span(kSaltForHkdf),
      base::as_byte_span(kInfoForHkdf));
  secret_.clear();

  Encryptor::Key derived_key(hashed, mojom::Algorithm::kAES256GCM);
  Finalize(InitStatus::kSuccess, kKeyTag, std::move(derived_key));
}

void SecretPortalKeyProvider::Finalize(InitStatus init_status) {
  CHECK_NE(init_status, InitStatus::kSuccess);
  Finalize(init_status, kKeyTag, std::nullopt);
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

  if (key.has_value()) {
    std::move(key_callback_).Run(tag, std::move(*key));
  } else {
    // TODO(crbug.com/389016528): Indicate whether this is a temporary or
    // permanent failure depending on the `init_status`.
    std::move(key_callback_)
        .Run(tag, base::unexpected(KeyError::kTemporarilyUnavailable));
  }

  read_watcher_.reset();
  read_fd_.reset();
  secret_.clear();

  base::UmaHistogramEnumeration(kUmaInitStatusEnum, init_status);

  std::string desktop = base::Environment::Create()
                            ->GetVar(base::nix::kXdgCurrentDesktopEnvVar)
                            .value_or(std::string());

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
