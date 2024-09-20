// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_COMMON_ENCRYPTOR_H_
#define COMPONENTS_OS_CRYPT_ASYNC_COMMON_ENCRYPTOR_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"

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

    static constexpr size_t kAES256GCMKeySize = 256u / 8u;

    // Mojo uses this public constructor for serialization.
    explicit Key(mojo::DefaultConstruct::Tag);

    Key(base::span<const uint8_t> key, const mojom::Algorithm& algo);

    bool operator==(const Key& other) const = default;

   private:
    friend class Encryptor;
    // OSCryptAsync and tests need to be able to Clone() keys.
    friend class OSCryptAsync;
    friend class TestOSCryptAsync;
    friend class EncryptorTestBase;
    friend struct mojo::StructTraits<os_crypt_async::mojom::KeyDataView,
                                     os_crypt_async::Encryptor::Key>;
    FRIEND_TEST_ALL_PREFIXES(EncryptorTestBase, MultipleKeys);
    FRIEND_TEST_ALL_PREFIXES(EncryptorTraitsTest, TraitsRoundTrip);

    Key(base::span<const uint8_t> key,
        const mojom::Algorithm& algo,
        bool encrypted);

    std::vector<uint8_t> Encrypt(base::span<const uint8_t> plaintext) const;
    std::optional<std::vector<uint8_t>> Decrypt(
        base::span<const uint8_t> ciphertext) const;

    Key Clone() const;

    // Algorithm. Can only be std::nullopt if the instance is in the process of
    // being serialized to/from mojo.
    std::optional<mojom::Algorithm> algorithm_;
    std::vector<uint8_t> key_;
    bool is_os_crypt_sync_compatible_ = false;
#if BUILDFLAG(IS_WIN)
    bool encrypted_ = false;
#endif
  };

  enum class Option {
    // No Encryptor options.
    kNone = 0,
    // Indicates that the Encryptor returned should be data-compatible with
    // OSCrypt Sync for both Encrypt and Decrypt operations. Note that Decrypt
    // operations are always backwards compatible with previous Encrypt
    // operations from OSCrypt Sync even if no option is specified: this option
    // only affects the behavior of Encrypt operations.
    kEncryptSyncCompat = 1,
  };

  // Flags that can be set by the Encryptor during a Decrypt call. Pass to a
  // Decrypt operation to obtain these flags.
  struct DecryptFlags {
    // Set by the Encryptor to indicate to the caller that the data that has
    // just been returned from the Decrypt operation should be re-encrypted with
    // a call to Encrypt, as the key has been rotated or a new key is available
    // that provides a different security level.
    bool should_reencrypt = false;
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
  [[nodiscard]] std::optional<std::vector<uint8_t>> EncryptString(
      const std::string& data) const;

  // Decrypt data previously encrypted using `EncryptData`. This can be called
  // on any thread. If a non-null `flags` is passed, then a set of flags is
  // returned to indicate additional information for the caller. See
  // `DecryptFlags` struct above.
  [[nodiscard]] std::optional<std::string> DecryptData(
      base::span<const uint8_t> data,
      DecryptFlags* flags = nullptr) const;

  // These four APIs are provided for backwards compatibility with OSCrypt. They
  // just call the above functions. For these functions, `flags` is optional.
  [[nodiscard]] bool EncryptString(const std::string& plaintext,
                                   std::string* ciphertext) const;
  [[nodiscard]] bool DecryptString(const std::string& ciphertext,
                                   std::string* plaintext,
                                   DecryptFlags* flags = nullptr) const;
  [[nodiscard]] bool EncryptString16(const std::u16string& plaintext,
                                     std::string* ciphertext) const;
  [[nodiscard]] bool DecryptString16(const std::string& ciphertext,
                                     std::u16string* plaintext,
                                     DecryptFlags* flags = nullptr) const;

  // Returns true if there is at least one key contained within the encryptor
  // that could be used for encryption, otherwise, it will return the value of
  // OSCrypt::IsEncryptionAvailable.
  bool IsEncryptionAvailable() const;

  // Returns true if there is at least one key contained within the encryptor
  // that might be able to decrypt data, otherwise it will return the value of
  // OSCrypt::IsEncryptionAvailable. Note that if this function returns true
  // then there is no guarantee that arbitrary data can be decrypted, as the
  // correct key to decrypt the data might not be available.
  bool IsDecryptionAvailable() const;

 private:
  friend class TestOSCryptAsync;
  friend class EncryptorTestBase;
  friend class OSCryptAsync;
  friend struct mojo::StructTraits<os_crypt_async::mojom::EncryptorDataView,
                                   os_crypt_async::Encryptor>;

  FRIEND_TEST_ALL_PREFIXES(EncryptorTraitsTest, TraitsRoundTrip);
  FRIEND_TEST_ALL_PREFIXES(EncryptorTestBase, Clone);

  // Create an encryptor with no keys or encryption provider. In this case, all
  // encryption operations will be delegated to OSCrypt.
  Encryptor();

  // Create an encryptor with a set of `keys`. The `provider_for_encryption`
  // specifies which provider is used for encryption, and must have a
  // corresponding key in `keys`.
  Encryptor(KeyRing keys, const std::string& provider_for_encryption);

  // Clone is used internally by the factory to vend instances.
  Encryptor Clone(Option option) const;

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
