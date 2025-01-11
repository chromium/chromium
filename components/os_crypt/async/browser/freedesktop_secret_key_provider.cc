// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/freedesktop_secret_key_provider.h"

#include <algorithm>
#include <memory>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "crypto/encryptor.h"
#include "crypto/kdf.h"
#include "dbus/message.h"
#include "dbus/object_path.h"

namespace os_crypt_async {

namespace {

// These constants are duplicated from the sync backend.
constexpr char kEncryptionTag[] = "v11";
constexpr char kSalt[] = "saltysalt";
constexpr size_t kDerivedKeySizeInBits = 128;
constexpr size_t kEncryptionIterations = 1;

template <typename ReplyArgs>
void CallMethod(dbus::ObjectProxy* object_proxy,
                const std::string& interface_name,
                const std::string& method_name,
                const DbusType& arguments,
                base::OnceCallback<void(std::optional<ReplyArgs>)> callback) {
  dbus::MethodCall method_call(interface_name, method_name);
  dbus::MessageWriter writer(&method_call);
  arguments.Write(&writer);
  object_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(
          [](const std::string& interface_name, const std::string& method_name,
             base::OnceCallback<void(std::optional<ReplyArgs>)> callback,
             dbus::Response* response) {
            if (!response) {
              std::move(callback).Run(std::nullopt);
              return;
            }
            dbus::MessageReader reader(response);
            ReplyArgs reply;
            if (!reply.Read(&reader)) {
              LOG(ERROR) << "Failed to read reply for " << interface_name << "."
                         << method_name << ": expected type "
                         << ReplyArgs::GetSignature() << " but got type "
                         << response->GetSignature();
              std::move(callback).Run(std::nullopt);
              return;
            }
            std::move(callback).Run(std::move(reply));
          },
          interface_name, method_name, std::move(callback)));
}

scoped_refptr<dbus::Bus> CreateBus() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SESSION;
  options.connection_type = dbus::Bus::PRIVATE;
  options.dbus_task_runner = dbus_thread_linux::GetTaskRunner();
  return base::MakeRefCounted<dbus::Bus>(options);
}

}  // namespace

FreedesktopSecretKeyProvider::FreedesktopSecretKeyProvider(
    bool use_for_encryption,
    const std::string& product_name,
    scoped_refptr<dbus::Bus> bus)
    : use_for_encryption_(use_for_encryption),
      product_name_(product_name),
      bus_(std::move(bus)) {
  if (!bus_) {
    bus_ = CreateBus();
  }
}

FreedesktopSecretKeyProvider::~FreedesktopSecretKeyProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FreedesktopSecretKeyProvider::GetKey(KeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  key_callback_ = std::move(callback);

  if (!secret_for_testing_.empty()) {
    DeriveKeyFromSecret(base::as_byte_span(secret_for_testing_));
    return;
  }

  dbus_utils::CheckForServiceAndStart(
      bus_, kSecretServiceName,
      base::BindOnce(&FreedesktopSecretKeyProvider::OnServiceStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool FreedesktopSecretKeyProvider::UseForEncryption() {
  return use_for_encryption_;
}

bool FreedesktopSecretKeyProvider::IsCompatibleWithOsCryptSync() {
  return true;
}

void FreedesktopSecretKeyProvider::OnServiceStarted(
    std::optional<bool> service_started) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!service_started.value_or(false)) {
    FinalizeFailure();
    return;
  }

  auto* service_proxy = bus_->GetObjectProxy(
      kSecretServiceName, dbus::ObjectPath(kSecretServicePath));
  CallMethod(service_proxy, kSecretServiceInterface, kMethodReadAlias,
             DbusString(kDefaultAlias),
             base::BindOnce(&FreedesktopSecretKeyProvider::OnReadAliasDefault,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnReadAliasDefault(
    std::optional<DbusObjectPath> collection_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!collection_path.has_value()) {
    FinalizeFailure();
    return;
  }
  if (collection_path->value().value() != "/") {
    default_collection_proxy_ =
        bus_->GetObjectProxy(kSecretServiceName, collection_path->value());
    OpenSession();
  } else {
    NOTIMPLEMENTED();
    FinalizeFailure();
  }
}

void FreedesktopSecretKeyProvider::OpenSession() {
  auto* service_proxy = bus_->GetObjectProxy(
      kSecretServiceName, dbus::ObjectPath(kSecretServicePath));
  CallMethod(service_proxy, kSecretServiceInterface, kMethodOpenSession,
             MakeDbusParameters(DbusString(kAlgorithmPlain),
                                MakeDbusVariant(DbusString(kInputPlain))),
             base::BindOnce(&FreedesktopSecretKeyProvider::OnOpenSession,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnOpenSession(
    std::optional<DbusParameters<DbusVariant, DbusObjectPath>> session_reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!session_reply.has_value()) {
    FinalizeFailure();
    return;
  }
  const auto& [_, result] = session_reply->value();
  session_proxy_ = bus_->GetObjectProxy(kSecretServiceName, result.value());
  session_opened_ = true;

  auto search_attrs = MakeDbusArray(MakeDbusDictEntry(
      DbusString(kApplicationAttributeKey), DbusString(kAppName)));

  CallMethod(default_collection_proxy_, kSecretCollectionInterface,
             kMethodSearchItems, search_attrs,
             base::BindOnce(&FreedesktopSecretKeyProvider::OnSearchItems,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnSearchItems(
    std::optional<DbusArray<DbusObjectPath>> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!results.has_value()) {
    FinalizeFailure();
    return;
  }

  if (results->value().empty()) {
    NOTIMPLEMENTED();
    FinalizeFailure();
    return;
  }

  auto* item_proxy = bus_->GetObjectProxy(kSecretServiceName,
                                          results->value().front().value());
  CallMethod(item_proxy, kSecretItemInterface, kMethodGetSecret,
             MakeDbusParameters(DbusObjectPath(session_proxy_->object_path())),
             base::BindOnce(&FreedesktopSecretKeyProvider::OnGetSecret,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FreedesktopSecretKeyProvider::OnGetSecret(
    std::optional<DbusSecret> secret_reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!secret_reply.has_value()) {
    FinalizeFailure();
    return;
  }

  const auto& [session_path, parameters, value, content_type] =
      secret_reply->value();
  const auto& secret_bytes = value.value();
  if (!secret_bytes) {
    FinalizeFailure();
    return;
  }
  if (secret_bytes->size() == 0) {
    LOG(ERROR) << "GetSecret returned an empty secret.";
    FinalizeFailure();
    return;
  }

  DeriveKeyFromSecret(base::span(*secret_bytes));
}

void FreedesktopSecretKeyProvider::DeriveKeyFromSecret(
    base::span<const uint8_t> secret) {
  static_assert(kDerivedKeySizeInBits % 8 == 0);
  std::array<uint8_t, kDerivedKeySizeInBits / 8> key_bytes;
  crypto::kdf::DeriveKeyPbkdf2HmacSha1(
      {kEncryptionIterations}, secret,
      base::as_byte_span(base::span_from_cstring(kSalt)), key_bytes,
      crypto::SubtlePassKey{});
  Encryptor::Key key(key_bytes, mojom::Algorithm::kAES128CBC);
  FinalizeSuccess(std::move(key));
}

void FreedesktopSecretKeyProvider::FinalizeSuccess(Encryptor::Key key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(key_callback_).Run(kEncryptionTag, std::move(key));
  CloseSession();
}

void FreedesktopSecretKeyProvider::FinalizeFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!key_callback_) {
    return;
  }
  std::move(key_callback_).Run(std::string(), std::nullopt);
  CloseSession();
}

void FreedesktopSecretKeyProvider::CloseSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (session_opened_) {
    CallMethod(session_proxy_, kSecretSessionInterface, kMethodClose,
               DbusVoid(), base::BindOnce([](std::optional<DbusVoid>) {}));
  }
}

}  // namespace os_crypt_async
