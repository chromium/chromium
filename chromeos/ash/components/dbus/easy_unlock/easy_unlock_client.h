// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_EASY_UNLOCK_EASY_UNLOCK_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_EASY_UNLOCK_EASY_UNLOCK_CLIENT_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/dbus/common/dbus_client.h"

namespace ash {

// Client for calling EasyUnlock dbus service. The service provides
// methods used by Easy Unlock for establishing secure communication channel
// over (unsecure) bluetooth with devices registered to unlock ChromeOS.
// Ideally, this would be done in Chrome, but unfortunatelly, the library used
// for wrapping and unwrapping messages sent over the communication channel
// depends on OpenSSL for encryption, which is not currently available in
// Chrome. To work around this, the message processing will be done in ChromeOS,
// where OpenSSL is already supported.
// TODO(tbarzic): Get rid of this client when Chrome switches from NSS to
// OpenSSL (http://crbug.com/338888).
class COMPONENT_EXPORT(ASH_DBUS_EASY_UNLOCK) EasyUnlockClient
    : public chromeos::DBusClient {
 public:
  // Callback for |GenerateEcP256KeyPair|. Carries the generated keys.
  // On error, arguments are empty strings.
  using KeyPairCallback =
      base::OnceCallback<void(const std::string& private_key,
                              const std::string& public_key)>;

  // Callback for D-Bus calls returning byte arrays as strings.
  // On error, |data| is empty.
  using DataCallback = base::OnceCallback<void(const std::string& data)>;

  // Parameters used to create a secure message.
  struct CreateSecureMessageOptions {
    CreateSecureMessageOptions();
    CreateSecureMessageOptions(const CreateSecureMessageOptions&) = delete;
    CreateSecureMessageOptions& operator=(const CreateSecureMessageOptions&) =
        delete;

    ~CreateSecureMessageOptions();

    // The key used to sign, and if needed, encrypt the message. If encryption
    // is required, the key must be symetric.
    std::string key;

    // Data associated with the message. The data will not actually be added to
    // the message, but it will be used while signing the message (the receiver
    // will use the same data to authenticate the signature).
    std::string associated_data;

    // Metadata added to the message header.
    std::string public_metadata;

    // The key id added to the message header. Has to be set if the message is
    // signed with private asymetric key. This value is used by the receiver to
    // identify the key that should be used to verify the signature.
    std::string verification_key_id;

    // Key id added to the message header. Used by the message receiver to
    // identify the key that should be used to decrypt the message.
    std::string decryption_key_id;

    // The encryption algorithm to use for encrypting the message.
    std::string encryption_type;

    // The algorithm to use to sign the message.
    std::string signature_type;
  };

  // Parameters used to unwrap a securemessage.
  struct UnwrapSecureMessageOptions {
    UnwrapSecureMessageOptions();
    UnwrapSecureMessageOptions(const UnwrapSecureMessageOptions&) = delete;
    UnwrapSecureMessageOptions& operator=(const UnwrapSecureMessageOptions&) =
        delete;

    ~UnwrapSecureMessageOptions();

    // The key used to authenticate message signature and, if needed, decrypt
    // the message. If the message is encrypted, only symetric key can be used.
    std::string key;

    // Data associated with the message. Message authentication will succeed
    // only if the message was created with the same associated data.
    std::string associated_data;

    // The encryption algorithm to use for decrypting the message.
    std::string encryption_type;

    // The algorithm that should be used to verify the message signature.
    std::string signature_type;
  };

  // Returns the global instance if initialized. May return null.
  static EasyUnlockClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  // Generates ECDSA key pair using P256 curve.
  // The created keys should only be used with this client.
  virtual void GenerateEcP256KeyPair(KeyPairCallback callback) = 0;

  // Converts public key bytes to format used by Easy Unlock.
  // |key_algorithm|: The asymmetric encryption algorithm with which the key is
  //     used.
  // |public_key|: The key that should be wrapped.
  // |callback|: The callback carrying the wrapped key.
  virtual void WrapPublicKey(const std::string& key_algorithm,
                             const std::string& public_key,
                             DataCallback callback) = 0;

  // Given a private and a public key, creates a symetric secret key using
  // EC Diffe-Hellman key exchange. The provided keys come from different
  // asymetric key pairs, and are expected to be in the same format as the ones
  // returned by |GenerateEcP256KeyAgreement|. Reversing key pairs from which
  // private and public key come generates the same secret key.
  virtual void PerformECDHKeyAgreement(const std::string& private_key,
                                       const std::string& public_key,
                                       DataCallback callback) = 0;

  // Creates signed and, if specified, encrypted message in format used by Easy
  // Unlock.
  // |payload|: The cleartext message body.
  // |options|: The message parameters used for creating the secure message.
  // |callback|: Called with the created message. On failure, the message will
  //     be empty.
  virtual void CreateSecureMessage(const std::string& payload,
                                   const CreateSecureMessageOptions& options,
                                   DataCallback callback) = 0;

  // Authenticates and, if specified, decrypts a secure message.
  // |message|: The message to unwrap. It is in the same format as the message
  //     returned by |CreateSecureMessage|.
  // |options|: The parameters that should be used to unwrap the message.
  // |callback|: Called with the cleartext message header and body in a signle
  //     protobuf. If the message could not be authenticated or decrypted, it
  //     will be called with an empty string.
  virtual void UnwrapSecureMessage(const std::string& message,
                                   const UnwrapSecureMessageOptions& options,
                                   DataCallback callback) = 0;

 protected:
  // Initialize() should be used instead.
  EasyUnlockClient();
  EasyUnlockClient(const EasyUnlockClient&) = delete;
  EasyUnlockClient& operator=(const EasyUnlockClient&) = delete;
  ~EasyUnlockClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_EASY_UNLOCK_EASY_UNLOCK_CLIENT_H_
