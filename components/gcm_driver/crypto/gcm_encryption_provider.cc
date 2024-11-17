// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_encryption_provider.h"

#include <memory>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/crypto/encryption_header_parsers.h"
#include "components/gcm_driver/crypto/gcm_decryption_result.h"
#include "components/gcm_driver/crypto/gcm_encryption_result.h"
#include "components/gcm_driver/crypto/gcm_key_store.h"
#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"
#include "components/gcm_driver/crypto/message_payload_parser.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "components/gcm_driver/crypto/proto/gcm_encryption_data.pb.h"
#include "crypto/ec_private_key.h"
#include "crypto/random.h"

namespace gcm {

namespace {

const char kEncryptionProperty[] = "encryption";
const char kCryptoKeyProperty[] = "crypto-key";
const char kInternalRawData[] = "_googRawData";

// Directory in the GCM Store in which the encryption database will be stored.
const base::FilePath::CharType kEncryptionDirectoryName[] =
    FILE_PATH_LITERAL("Encryption");

IncomingMessage CreateMessageWithId(const std::string& message_id) {
  IncomingMessage message;
  message.message_id = message_id;
  return message;
}

}  // namespace

GCMEncryptionProvider::GCMEncryptionProvider() = default;

GCMEncryptionProvider::~GCMEncryptionProvider() = default;

// static
const char GCMEncryptionProvider::kContentEncodingProperty[] =
    "content-encoding";

// static
const char GCMEncryptionProvider::kContentCodingAes128Gcm[] = "aes128gcm";

void GCMEncryptionProvider::Init(
    const base::FilePath& store_path,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner) {
  DCHECK(!key_store_);

  base::FilePath encryption_store_path = store_path;

  // |store_path| can be empty in tests, which means that the database should
  // be created in memory rather than on-disk.
  if (!store_path.empty())
    encryption_store_path = store_path.Append(kEncryptionDirectoryName);

  key_store_ = std::make_unique<GCMKeyStore>(encryption_store_path,
                                             blocking_task_runner);
}

void GCMEncryptionProvider::GetEncryptionInfo(
    const std::string& app_id,
    const std::string& authorized_entity,
    EncryptionInfoCallback callback) {
  DCHECK(key_store_);
  key_store_->GetKeys(
      app_id, authorized_entity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMEncryptionProvider::DidGetEncryptionInfo,
                     weak_ptr_factory_.GetWeakPtr(), app_id, authorized_entity,
                     std::move(callback)));
}

void GCMEncryptionProvider::DidGetEncryptionInfo(
    const std::string& app_id,
    const std::string& authorized_entity,
    EncryptionInfoCallback callback,
    std::unique_ptr<crypto::ECPrivateKey> key,
    const std::string& auth_secret) {
  if (!key) {
    key_store_->CreateKeys(
        app_id, authorized_entity,
        base::BindOnce(&GCMEncryptionProvider::DidCreateEncryptionInfo,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  std::string public_key;
  const bool success = GetRawPublicKey(*key, &public_key);
  DCHECK(success);
  std::move(callback).Run(public_key, auth_secret);
}

void GCMEncryptionProvider::RemoveEncryptionInfo(
    const std::string& app_id,
    const std::string& authorized_entity,
    base::OnceClosure callback) {
  DCHECK(key_store_);
  key_store_->RemoveKeys(app_id, authorized_entity, std::move(callback));
}

bool GCMEncryptionProvider::IsEncryptedMessage(
    const IncomingMessage& message) const {
  // Messages that explicitly specify their content coding to be "aes128gcm"
  // indicate that they use draft-ietf-webpush-encryption-08.
  auto content_encoding_iter = message.data.find(kContentEncodingProperty);
  if (content_encoding_iter != message.data.end() &&
      content_encoding_iter->second == kContentCodingAes128Gcm) {
    return true;
  }

  // The Web Push protocol requires the encryption and crypto-key properties to
  // be set, and the raw_data field to be populated with the payload.
  if (message.data.find(kEncryptionProperty) == message.data.end() ||
      message.data.find(kCryptoKeyProperty) == message.data.end())
    return false;

  return message.raw_data.size() > 0;
}

void GCMEncryptionProvider::DecryptMessage(const std::string& app_id,
                                           const IncomingMessage& message,
                                           DecryptMessageCallback callback) {
  DCHECK(key_store_);
  if (!IsEncryptedMessage(message)) {
    std::move(callback).Run(GCMDecryptionResult::UNENCRYPTED, message);
    return;
  }

  std::string salt, public_key, ciphertext;
  GCMMessageCryptographer::Version version;
  uint32_t record_size;

  auto content_encoding_iter = message.data.find(kContentEncodingProperty);
  if (content_encoding_iter != message.data.end() &&
      content_encoding_iter->second == kContentCodingAes128Gcm) {
    // The message follows encryption per draft-ietf-webpush-encryption-08. Use
    // the binary header of the message to derive the values.

    auto parser = std::make_unique<MessagePayloadParser>(message.raw_data);
    if (!parser->IsValid()) {
      // Attempt to parse base64 encoded internal raw data.
      auto raw_data_iter = message.data.find(kInternalRawData);
      std::string raw_data;
      if (raw_data_iter == message.data.end() ||
          !base::Base64Decode(raw_data_iter->second, &raw_data) ||
          !(parser = std::make_unique<MessagePayloadParser>(raw_data))
               ->IsValid()) {
        DLOG(ERROR) << "Unable to parse the message's binary header";
        std::move(callback).Run(parser->GetFailureReason(),
                                CreateMessageWithId(message.message_id));
        return;
      }
    }

    salt = parser->salt();
    public_key = parser->public_key();
    record_size = parser->record_size();
    ciphertext = parser->ciphertext();
    version = GCMMessageCryptographer::Version::DRAFT_08;
  } else {
    // The message follows encryption per draft-ietf-webpush-encryption-03. Use
    // the Encryption and Crypto-Key header values to derive the values.

    const auto& encryption_header = message.data.find(kEncryptionProperty);
    CHECK(encryption_header != message.data.end(), base::NotFatalUntil::M130);

    const auto& crypto_key_header = message.data.find(kCryptoKeyProperty);
    CHECK(crypto_key_header != message.data.end(), base::NotFatalUntil::M130);

    EncryptionHeaderIterator encryption_header_iterator(
        encryption_header->second.begin(), encryption_header->second.end());
    if (!encryption_header_iterator.GetNext()) {
      DLOG(ERROR) << "Unable to parse the value of the Encryption header";
      std::move(callback).Run(GCMDecryptionResult::INVALID_ENCRYPTION_HEADER,
                              CreateMessageWithId(message.message_id));
      return;
    }

    if (encryption_header_iterator.salt().size() !=
        GCMMessageCryptographer::kSaltSize) {
      DLOG(ERROR) << "Invalid values supplied in the Encryption header";
      std::move(callback).Run(GCMDecryptionResult::INVALID_ENCRYPTION_HEADER,
                              CreateMessageWithId(message.message_id));
      return;
    }

    salt = encryption_header_iterator.salt();
    record_size = encryption_header_iterator.rs();

    CryptoKeyHeaderIterator crypto_key_header_iterator(
        crypto_key_header->second.begin(), crypto_key_header->second.end());
    if (!crypto_key_header_iterator.GetNext()) {
      DLOG(ERROR) << "Unable to parse the value of the Crypto-Key header";
      std::move(callback).Run(GCMDecryptionResult::INVALID_CRYPTO_KEY_HEADER,
                              CreateMessageWithId(message.message_id));
      return;
    }

    // Ignore values that don't include the "dh" property. When using VAPID, it
    // is valid for the application server to supply multiple values.
    while (crypto_key_header_iterator.dh().empty() &&
           crypto_key_header_iterator.GetNext()) {
    }

    bool valid_crypto_key_header = false;

    if (!crypto_key_header_iterator.dh().empty()) {
      public_key = crypto_key_header_iterator.dh();
      valid_crypto_key_header = true;

      // Guard against the "dh" property being included more than once.
      while (crypto_key_header_iterator.GetNext()) {
        if (crypto_key_header_iterator.dh().empty())
          continue;

        valid_crypto_key_header = false;
        break;
      }
    }

    if (!valid_crypto_key_header) {
      DLOG(ERROR) << "Invalid values supplied in the Crypto-Key header";
      std::move(callback).Run(GCMDecryptionResult::INVALID_CRYPTO_KEY_HEADER,
                              CreateMessageWithId(message.message_id));
      return;
    }

    ciphertext = message.raw_data;
    version = GCMMessageCryptographer::Version::DRAFT_03;
  }

  // Use |fallback_to_empty_authorized_entity|, since this message might have
  // been sent to either an InstanceID token or a non-InstanceID registration.
  key_store_->GetKeys(
      app_id, message.sender_id /* authorized_entity */,
      true /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMEncryptionProvider::DecryptMessageWithKey,
                     weak_ptr_factory_.GetWeakPtr(), message.message_id,
                     message.collapse_key, message.sender_id, std::move(salt),
                     std::move(public_key), record_size, std::move(ciphertext),
                     version, std::move(callback)));
}  // namespace gcm

void GCMEncryptionProvider::EncryptMessage(const std::string& app_id,
                                           const std::string& authorized_entity,
                                           const std::string& p256dh,
                                           const std::string& auth_secret,
                                           const std::string& message,
                                           EncryptMessageCallback callback) {
  DCHECK(key_store_);
  key_store_->GetKeys(
      app_id, authorized_entity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMEncryptionProvider::EncryptMessageWithKey,
                     weak_ptr_factory_.GetWeakPtr(), app_id, authorized_entity,
                     p256dh, auth_secret, message, std::move(callback)));
}

void GCMEncryptionProvider::DidCreateEncryptionInfo(
    EncryptionInfoCallback callback,
    std::unique_ptr<crypto::ECPrivateKey> key,
    const std::string& auth_secret) {
  if (!key) {
    std::move(callback).Run(std::string() /* p256dh */,
                            std::string() /* auth_secret */);
    return;
  }

  std::string public_key;
  const bool success = GetRawPublicKey(*key, &public_key);
  DCHECK(success);
  std::move(callback).Run(public_key, auth_secret);
}

void GCMEncryptionProvider::DecryptMessageWithKey(
    const std::string& message_id,
    const std::string& collapse_key,
    const std::string& sender_id,
    const std::string& salt,
    const std::string& public_key,
    uint32_t record_size,
    const std::string& ciphertext,
    GCMMessageCryptographer::Version version,
    DecryptMessageCallback callback,
    std::unique_ptr<crypto::ECPrivateKey> key,
    const std::string& auth_secret) {
  if (!key) {
    DLOG(ERROR) << "Unable to retrieve the keys for the incoming message.";
    std::move(callback).Run(GCMDecryptionResult::NO_KEYS,
                            CreateMessageWithId(message_id));
    return;
  }

  std::string shared_secret;
  if (!ComputeSharedP256Secret(*key, public_key, &shared_secret)) {
    DLOG(ERROR) << "Unable to calculate the shared secret.";
    std::move(callback).Run(GCMDecryptionResult::INVALID_SHARED_SECRET,
                            CreateMessageWithId(message_id));
    return;
  }

  std::string plaintext;

  GCMMessageCryptographer cryptographer(version);

  std::string exported_public_key;
  const bool success = GetRawPublicKey(*key, &exported_public_key);
  DCHECK(success);
  if (!cryptographer.Decrypt(exported_public_key, public_key, shared_secret,
                             auth_secret, salt, ciphertext, record_size,
                             &plaintext)) {
    DLOG(ERROR) << "Unable to decrypt the incoming data.";
    std::move(callback).Run(GCMDecryptionResult::INVALID_PAYLOAD,
                            CreateMessageWithId(message_id));
    return;
  }

  IncomingMessage decrypted_message;
  decrypted_message.message_id = message_id;
  decrypted_message.collapse_key = collapse_key;
  decrypted_message.sender_id = sender_id;
  decrypted_message.raw_data.swap(plaintext);
  decrypted_message.decrypted = true;

  // There must be no data associated with the decrypted message at this point,
  // to make sure that we don't end up in an infinite decryption loop.
  DCHECK_EQ(0u, decrypted_message.data.size());

  std::move(callback).Run(version == GCMMessageCryptographer::Version::DRAFT_03
                              ? GCMDecryptionResult::DECRYPTED_DRAFT_03
                              : GCMDecryptionResult::DECRYPTED_DRAFT_08,
                          std::move(decrypted_message));
}

void GCMEncryptionProvider::EncryptMessageWithKey(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& p256dh,
    const std::string& auth_secret,
    const std::string& message,
    EncryptMessageCallback callback,
    std::unique_ptr<crypto::ECPrivateKey> key,
    const std::string& sender_auth_secret) {
  if (!key) {
    DLOG(ERROR) << "Unable to retrieve the keys for the outgoing message.";
    std::move(callback).Run(GCMEncryptionResult::NO_KEYS, std::string());
    return;
  }

  // Creates a cryptographically secure salt of |salt_size| octets in size,
  // and calculate the shared secret for the message.
  std::string salt(16, '\0');
  crypto::RandBytes(base::as_writable_byte_span(salt));

  std::string shared_secret;
  if (!ComputeSharedP256Secret(*key, p256dh, &shared_secret)) {
    DLOG(ERROR) << "Unable to calculate the shared secret.";
    std::move(callback).Run(GCMEncryptionResult::INVALID_SHARED_SECRET,
                            std::string());
    return;
  }

  size_t record_size;
  std::string ciphertext;

  GCMMessageCryptographer cryptographer(
      GCMMessageCryptographer::Version::DRAFT_08);

  std::string sender_public_key;
  bool success = GetRawPublicKey(*key, &sender_public_key);
  DCHECK(success);
  if (!cryptographer.Encrypt(p256dh, sender_public_key, shared_secret,
                             auth_secret, salt, message, &record_size,
                             &ciphertext)) {
    DLOG(ERROR) << "Unable to encrypt the incoming data.";
    std::move(callback).Run(GCMEncryptionResult::ENCRYPTION_FAILED,
                            std::string());
    return;
  }

  // Construct encryption header.
  uint32_t rs = record_size;
  std::string rs_str(sizeof(rs), 0u);
  base::as_writable_byte_span(rs_str).copy_from(base::U32ToBigEndian(rs));

  uint8_t key_length = sender_public_key.size();
  std::string key_length_str(sizeof(key_length), 0u);
  base::as_writable_byte_span(key_length_str)
      .copy_from(base::U8ToBigEndian(key_length));

  std::string payload = base::StrCat(
      {salt, rs_str, key_length_str, sender_public_key, ciphertext});
  std::move(callback).Run(GCMEncryptionResult::ENCRYPTED_DRAFT_08,
                          std::move(payload));
}

}  // namespace gcm
