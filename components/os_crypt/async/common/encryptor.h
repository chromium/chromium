// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_COMMON_ENCRYPTOR_H_
#define COMPONENTS_OS_CRYPT_ASYNC_COMMON_ENCRYPTOR_H_

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}  // namespace mojo

namespace os_crypt_async {

namespace mojom {
enum class Algorithm;
class EncryptorDataView;
class KeyDataView;
}  // namespace mojom

class EncryptorTestBase;
class OSCryptAsync;
class TestOSCryptAsync;

// This class is used for data encryption. A thread-safe instance can be
// obtained by calling `os_crypt_async::OSCryptAsync::GetInstance`.
class COMPONENT_EXPORT(OS_CRYPT_ASYNC) Encryptor {
 public:
  // A class used by the Encryptor to hold an encryption key and carry out
  // encryption and decryption operations using the specified Algorithm and
  // encryption key.
  class COMPONENT_EXPORT(OS_CRYPT_ASYNC) Key {
   public:
    // Moveable, not copyable.
    Key(Key&& other);
    Key& operator=(Key&& other);
    Key(const Key&) = delete;
    Key& operator=(const Key&) = delete;

    ~Key();

    static const size_t kAES256GCMKeySize = 256u / 8u;

    // Mojo uses this public constructor for serialization.
    explicit Key(mojo::DefaultConstruct::Tag);

    Key(base::span<const uint8_t> key, const mojom::Algorithm& algo);

    bool operator==(const Key& other) const = default;

   private:
    friend class Encryptor;
    // OSCryptAsync and tests need to be able to Clone() keys.
    friend class OSCryptAsync;
    friend struct mojo::StructTraits<os_crypt_async::mojom::KeyDataView,
                                     os_crypt_async::Encryptor::Key>;
    FRIEND_TEST_ALL_PREFIXES(EncryptorTestBase, MultipleKeys);
    FRIEND_TEST_ALL_PREFIXES(EncryptorTraitsTest, TraitsRoundTrip);

    std::vector<uint8_t> Encrypt(base::span<const uint8_t> plaintext) const;
    absl::optional<std::vector<uint8_t>> Decrypt(
        base::span<const uint8_t> ciphertext) const;

    Key Clone() const;

    // Algorithm. Can only be absl::nullopt if the instance is in the process of
    // being serialized to/from mojo.
    absl::optional<mojom::Algorithm> algorithm_;
    std::vector<uint8_t> key_;
  };

  using KeyRing = std::map</*tag=*/std::string, Key>;

  // Mojo uses this public constructor for serialization.
  explicit Encryptor(mojo::DefaultConstruct::Tag);

  ~Encryptor();

  // Moveable, not copyable.
  Encryptor(Encryptor&& other);
  Encryptor& operator=(Encryptor&& other);
  Encryptor(const Encryptor&) = delete;
  Encryptor& operator=(const Encryptor&) = delete;

  // Encrypt a string with the current Encryptor configuration. This can be
  // called on any thread.
  [[nodiscard]] absl::optional<std::vector<uint8_t>> EncryptString(
      const std::string& data) const;

  // Decrypt data previously encrypted using `EncryptData`. This can be called
  // on any thread.
  [[nodiscard]] absl::optional<std::string> DecryptData(
      base::span<const uint8_t> data) const;

  // These two APIs are provided for backwards compatibility with OSCrypt. They
  // just call the above functions. The two sets of functions are compatible
  // with each other.
  [[nodiscard]] bool EncryptString(const std::string& plaintext,
                                   std::string* ciphertext) const;
  [[nodiscard]] bool DecryptString(const std::string& ciphertext,
                                   std::string* plaintext) const;

 private:
  friend class TestOSCryptAsync;
  friend class EncryptorTestBase;
  friend class OSCryptAsync;
  friend struct mojo::StructTraits<os_crypt_async::mojom::EncryptorDataView,
                                   os_crypt_async::Encryptor>;

  FRIEND_TEST_ALL_PREFIXES(EncryptorTraitsTest, TraitsRoundTrip);

  // Create an encryptor with no keys or encryption provider. In this case, all
  // encryption operations will be delegated to OSCrypt.
  Encryptor();

  // Create an encryptor with a set of `keys`. The `provider_for_encryption`
  // specifies which provider is used for encryption, and must have a
  // corresponding key in `keys`.
  Encryptor(KeyRing keys, const std::string& provider_for_encryption);

  // Clone is used by the factory to vend instances.
  Encryptor Clone() const;

  // A KeyRing consists of a set of provider names and Key values. Encrypted
  // data is always tagged with the provider name and this is used to look up
  // the correct key to use for decryption.
  KeyRing keys_;

  // The provider with this tag is used when encrypting any new data, the Key to
  // use for the encryption is looked up from the entry in the KeyRing.
  std::string provider_for_encryption_;
};

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_COMMON_ENCRYPTOR_H_
