// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_SECURE_MESSAGE_DELEGATE_H_
#define CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_SECURE_MESSAGE_DELEGATE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "third_party/securemessage/proto/securemessage.pb.h"

namespace ash::multidevice {

// Interface of delegate responsible for cryptographic operations based on the
// secure message library. This interface is asynchronous as the current
// implementation on ChromeOS communicates with a daemon process over IPC.
class SecureMessageDelegate {
 public:
  // Fields specifying how to create a SecureMessage.
  struct CreateOptions {
    CreateOptions();
    CreateOptions(const CreateOptions& other);
    ~CreateOptions();

    // The scheme used to encrypt the message.
    securemessage::EncScheme encryption_scheme;
    // The scheme used to sign the message.
    securemessage::SigScheme signature_scheme;
    // Additional data that is used as part of the signature computation but not
    // included in the message contents.
    std::string associated_data;
    // Plain-text data included in the message header.
    std::string public_metadata;
    // Identifies the key to use for verifying the message signature.
    std::string verification_key_id;
    // Identifies the key to use for decrypting the message.
    std::string decryption_key_id;
  };

  // Fields specifying how to unwrap a SecureMessage.
  struct UnwrapOptions {
    UnwrapOptions();
    ~UnwrapOptions();

    // The scheme used to decrypt the message.
    securemessage::EncScheme encryption_scheme;
    // The scheme used to verify the message signature.
    securemessage::SigScheme signature_scheme;
    // Additional data that is used as part of the signature computation but not
    // included in the message contents.
    std::string associated_data;
  };

  SecureMessageDelegate();
  virtual ~SecureMessageDelegate();

  // Generates a new asymmetric key pair.
  typedef base::OnceCallback<void(const std::string& public_key,
                                  const std::string& private_key)>
      GenerateKeyPairCallback;
  virtual void GenerateKeyPair(GenerateKeyPairCallback callback) = 0;

  // Derives a symmetric key from our private key and the remote device's
  // public key.
  typedef base::OnceCallback<void(const std::string& derived_key)>
      DeriveKeyCallback;
  virtual void DeriveKey(const std::string& private_key,
                         const std::string& public_key,
                         DeriveKeyCallback callback) = 0;

  // Creates a new secure message with a |payload| given the |key| and
  // |create_options| specifying the cryptographic details.
  // |callback| will be invoked with the serialized SecureMessage upon success
  // or the empty string upon failure.
  typedef base::OnceCallback<void(const std::string& secure_message)>
      CreateSecureMessageCallback;
  virtual void CreateSecureMessage(const std::string& payload,
                                   const std::string& key,
                                   const CreateOptions& create_options,
                                   CreateSecureMessageCallback callback) = 0;

  // Unwraps |secure_message| given the |key| and |unwrap_options| specifying
  // the cryptographic details.
  // |callback| will be invoked with true for the |verified| argument if the
  // message was verified and decrypted successfully. The |payload| and
  // |header| fields will be non-empty if the message was verified successfully.
  typedef base::OnceCallback<void(bool verified,
                                  const std::string& payload,
                                  const securemessage::Header& header)>
      UnwrapSecureMessageCallback;
  virtual void UnwrapSecureMessage(const std::string& serialized_message,
                                   const std::string& key,
                                   const UnwrapOptions& unwrap_options,
                                   UnwrapSecureMessageCallback callback) = 0;
};

}  // namespace ash::multidevice

#endif  // CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_SECURE_MESSAGE_DELEGATE_H_
