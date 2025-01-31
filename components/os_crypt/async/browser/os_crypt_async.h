// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_BROWSER_OS_CRYPT_ASYNC_H_
#define COMPONENTS_OS_CRYPT_ASYNC_BROWSER_OS_CRYPT_ASYNC_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/os_crypt/async/browser/key_provider.h"
#include "components/os_crypt/async/common/encryptor.h"

namespace os_crypt_async {

// This class is responsible for vending Encryptor instances.
class COMPONENT_EXPORT(OS_CRYPT_ASYNC) OSCryptAsync {
 public:
  using InitCallback = base::OnceCallback<void(Encryptor, bool result)>;

  // Higher precedence providers will be used for encryption over lower
  // preference ones.
  using Precedence = size_t;

  // Create an OSCryptAsync with the defined list of `providers`.
  //
  // Each provider consists of a pair that defines the precedence of the
  // provider and the provider implementation.
  //
  // The provider with the highest precedence that supplies a valid Key, and
  // also signals that it can be used for encryption by returning `true` from
  // the `UseForEncryption` interface method will be used for encryption.
  //
  // If no providers are available for encryption, legacy OSCrypt will be used
  // for encryption.
  //
  // Any provider that supplies a Key, regardless of their precedence or whether
  // or not they signal `UseForEncryption`, will make that Key available for
  // decryption of data that was previously encrypted with the same Key.
  explicit OSCryptAsync(
      std::vector<std::pair<Precedence, std::unique_ptr<KeyProvider>>>
          providers);
  virtual ~OSCryptAsync();

  // Not moveable. Not copyable.
  OSCryptAsync(OSCryptAsync&& other) = delete;
  OSCryptAsync& operator=(OSCryptAsync&& other) = delete;
  OSCryptAsync(const OSCryptAsync&) = delete;
  OSCryptAsync& operator=(const OSCryptAsync&) = delete;

  // Obtain an Encryptor instance. Can be called multiple times, each one will
  // get a valid instance once the initialization has completed, on the
  // `callback`. `option` determines characteristics of the resulting Encryptor
  // instance returned in the callback, see encryptor.h. Must be called on the
  // same sequence that the OSCryptAsync object was created on. Destruction of
  // the `base::CallbackListSubscription` will cause the callback not to run,
  // see `base/callback_list.h`.
  [[nodiscard]] virtual base::CallbackListSubscription GetInstance(
      InitCallback callback,
      Encryptor::Option option);

  // Same as the `GetInstance` method above but uses a default option.
  [[nodiscard]] base::CallbackListSubscription GetInstance(
      InitCallback callback);

 private:
  using ProviderIterator =
      std::vector<std::unique_ptr<KeyProvider>>::const_iterator;

  void CallbackHelper(InitCallback callback, Encryptor::Option option) const;
  void HandleKey(ProviderIterator current,
                 const std::string& tag,
                 std::optional<Encryptor::Key> key);

  std::unique_ptr<Encryptor> encryptor_instance_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool is_initialized_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_initializing_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  const std::vector<std::unique_ptr<KeyProvider>> providers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::OnceClosureList callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);
  Encryptor::KeyRing key_ring_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string provider_for_encryption_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OSCryptAsync> weak_factory_{this};
};

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_BROWSER_OS_CRYPT_ASYNC_H_
