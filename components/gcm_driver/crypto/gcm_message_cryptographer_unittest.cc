// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"

#include <memory>
#include <string_view>

#include "base/base64url.h"
#include "base/big_endian.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "components/gcm_driver/crypto/message_payload_parser.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "crypto/ec_private_key.h"
#include "crypto/random.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

// Example plaintext data to use in the tests.
const char kExamplePlaintext[] = "Example plaintext";

// Expected sizes of the different input given to the cryptographer.
constexpr size_t kEcdhSharedSecretSize = 32;
constexpr size_t kAuthSecretSize = 16;
constexpr size_t kSaltSize = 16;

// Keying material for both parties as P-256 EC points. Used to make sure that
// the test vectors are reproducible.
const unsigned char kCommonSenderPublicKey[] = {
    0x04, 0x05, 0x3C, 0xA1, 0xB9, 0xA5, 0xAB, 0xB8, 0x2D, 0x88, 0x48,
    0x82, 0xC9, 0x49, 0x19, 0x91, 0xD5, 0xFD, 0xD1, 0x92, 0xDB, 0xA7,
    0x7E, 0x70, 0x48, 0x37, 0x41, 0xCD, 0x90, 0x05, 0x80, 0xDF, 0x65,
    0x9A, 0xA1, 0x1A, 0x04, 0xF1, 0x98, 0x25, 0xF2, 0xC2, 0x13, 0x5D,
    0xD9, 0x72, 0x35, 0x75, 0x24, 0xF9, 0xFF, 0x25, 0xD1, 0xBC, 0x84,
    0x46, 0x4E, 0x88, 0x08, 0x55, 0x70, 0x9F, 0xA7, 0x07, 0xD9};
static_assert(std::size(kCommonSenderPublicKey) == 65,
              "Raw P-256 public keys must be 65 bytes in size.");

const unsigned char kCommonRecipientPublicKey[] = {
    0x04, 0x35, 0x02, 0x67, 0xB9, 0x10, 0x8F, 0x9B, 0xF1, 0x85, 0xF5,
    0x1B, 0xD7, 0xA4, 0xEF, 0xBD, 0x28, 0xB3, 0x11, 0x40, 0xBA, 0xD0,
    0xEE, 0xB2, 0x97, 0xDA, 0x6A, 0x93, 0x2D, 0x26, 0x45, 0xBD, 0xB2,
    0x9A, 0x9F, 0xB8, 0x19, 0xD8, 0x21, 0x6F, 0x66, 0xE3, 0xF6, 0x0B,
    0x74, 0xB2, 0x28, 0x38, 0xDC, 0xA7, 0x8A, 0x58, 0x0D, 0x56, 0x47,
    0x3E, 0xD0, 0x5B, 0x5C, 0x93, 0x4E, 0xB3, 0x89, 0x87, 0x64};
static_assert(std::size(kCommonRecipientPublicKey) == 65,
              "Raw P-256 public keys must be 65 bytes in size.");

const unsigned char kCommonRecipientPrivateKey[] = {
    0x30, 0x81, 0x87, 0x02, 0x01, 0x00, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86,
    0x48, 0xCE, 0x3D, 0x02, 0x01, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D,
    0x03, 0x01, 0x07, 0x04, 0x6D, 0x30, 0x6B, 0x02, 0x01, 0x01, 0x04, 0x20,
    0x16, 0xCC, 0xB4, 0x37, 0xA3, 0x04, 0x0C, 0x28, 0xDE, 0x56, 0x77, 0x27,
    0x0B, 0xD8, 0x1E, 0x82, 0xD7, 0x7F, 0x07, 0xA6, 0x43, 0x6E, 0x70, 0xDD,
    0x9C, 0x3C, 0xF1, 0x2C, 0x93, 0xE3, 0x37, 0xD1, 0xA1, 0x44, 0x03, 0x42,
    0x00, 0x04, 0x35, 0x02, 0x67, 0xB9, 0x10, 0x8F, 0x9B, 0xF1, 0x85, 0xF5,
    0x1B, 0xD7, 0xA4, 0xEF, 0xBD, 0x28, 0xB3, 0x11, 0x40, 0xBA, 0xD0, 0xEE,
    0xB2, 0x97, 0xDA, 0x6A, 0x93, 0x2D, 0x26, 0x45, 0xBD, 0xB2, 0x9A, 0x9F,
    0xB8, 0x19, 0xD8, 0x21, 0x6F, 0x66, 0xE3, 0xF6, 0x0B, 0x74, 0xB2, 0x28,
    0x38, 0xDC, 0xA7, 0x8A, 0x58, 0x0D, 0x56, 0x47, 0x3E, 0xD0, 0x5B, 0x5C,
    0x93, 0x4E, 0xB3, 0x89, 0x87, 0x64};

const unsigned char kCommonAuthSecret[] = {0x25, 0xF2, 0xC2, 0xB8, 0x19, 0xD8,
                                           0xFD, 0x35, 0x97, 0xDF, 0xFB, 0x5E,
                                           0xF6, 0x0B, 0xD7, 0xA4};
static_assert(std::size(kCommonAuthSecret) == 16,
              "Auth secrets must be 16 bytes in size.");

// Test vectors containing reference input for draft-ietf-webpush-encryption
// that was created using an separate JavaScript implementation of the draft.
struct TestVector {
  const char* const input;
  const unsigned char ecdh_shared_secret[kEcdhSharedSecretSize];
  const unsigned char auth_secret[kAuthSecretSize];
  const unsigned char salt[kSaltSize];
  size_t record_size;
  const char* const output;
};

const TestVector kEncryptionTestVectorsDraft03[] = {
    // Simple message.
    {"Hello, world!",
     {0x0B, 0x32, 0xE2, 0xD1, 0x6A, 0xBF, 0x4F, 0x2C, 0x49, 0xEA, 0xF7,
      0x5D, 0x71, 0x7D, 0x89, 0xA9, 0xA7, 0x5E, 0x21, 0xB2, 0xB5, 0x51,
      0xE6, 0x4C, 0x08, 0x68, 0xD3, 0x6F, 0x8F, 0x72, 0x7E, 0x14},
     {0xD3, 0xF2, 0x78, 0xBD, 0x8D, 0xDD, 0x84, 0x99, 0x66, 0x08, 0xD7, 0x0F,
      0xBA, 0x9B, 0x60, 0xFC},
     {0x15, 0x4A, 0xD7, 0x73, 0x92, 0xBD, 0x3B, 0xCF, 0x6F, 0x98, 0xDC, 0x9B,
      0x8B, 0x56, 0xFB, 0xBD},
     4096,
     "T4SXCyj84drA6wRaBNLGDMzeyOEBWjsIEkS2ros6Aw"},
    // Empty message.
    {"",
     {0x3F, 0xD8, 0x95, 0x2C, 0xA2, 0x11, 0xBD, 0x7B, 0x57, 0xB2, 0x00,
      0xBD, 0x57, 0x68, 0x3F, 0xF0, 0x14, 0x57, 0x5F, 0xB1, 0x9F, 0x15,
      0x4F, 0x11, 0xF0, 0x4D, 0xA2, 0xE8, 0x4C, 0xEA, 0x74, 0x3B},
     {0xB1, 0xE1, 0xC7, 0x32, 0x4C, 0xAA, 0x56, 0x32, 0x68, 0x20, 0x0F, 0x26,
      0x3F, 0x48, 0x4D, 0x99},
     {0xE9, 0x39, 0x45, 0xBC, 0x96, 0x96, 0x88, 0x76, 0xFC, 0xA1, 0xAD, 0xE4,
      0x9D, 0x28, 0xF3, 0x73},
     4096,
     "8s-Tzq8Cn_eobL6uEcNDXL7K"}};

const TestVector kEncryptionTestVectorsDraft08[] = {
    // Simple message.
    {"Hello, world!",
     {0x0B, 0x32, 0xE2, 0xD1, 0x6A, 0xBF, 0x4F, 0x2C, 0x49, 0xEA, 0xF7,
      0x5D, 0x71, 0x7D, 0x89, 0xA9, 0xA7, 0x5E, 0x21, 0xB2, 0xB5, 0x51,
      0xE6, 0x4C, 0x08, 0x68, 0xD3, 0x6F, 0x8F, 0x72, 0x7E, 0x14},
     {0xD3, 0xF2, 0x78, 0xBD, 0x8D, 0xDD, 0x84, 0x99, 0x66, 0x08, 0xD7, 0x0F,
      0xBA, 0x9B, 0x60, 0xFC},
     {0x15, 0x4A, 0xD7, 0x73, 0x92, 0xBD, 0x3B, 0xCF, 0x6F, 0x98, 0xDC, 0x9B,
      0x8B, 0x56, 0xFB, 0xBD},
     4096,
     "3biYN3Aa30D30bKJMdGlEyYPrz7Wg293NYc31rb6"},
    // Empty message.
    {"",
     {0x3F, 0xD8, 0x95, 0x2C, 0xA2, 0x11, 0xBD, 0x7B, 0x57, 0xB2, 0x00,
      0xBD, 0x57, 0x68, 0x3F, 0xF0, 0x14, 0x57, 0x5F, 0xB1, 0x9F, 0x15,
      0x4F, 0x11, 0xF0, 0x4D, 0xA2, 0xE8, 0x4C, 0xEA, 0x74, 0x3B},
     {0xB1, 0xE1, 0xC7, 0x32, 0x4C, 0xAA, 0x56, 0x32, 0x68, 0x20, 0x0F, 0x26,
      0x3F, 0x48, 0x4D, 0x99},
     {0xE9, 0x39, 0x45, 0xBC, 0x96, 0x96, 0x88, 0x76, 0xFC, 0xA1, 0xAD, 0xE4,
      0x9D, 0x28, 0xF3, 0x73},
     4096,
     "5OXY345WYPyIvsF7hx4swuA"}};

const TestVector kDecryptionTestVectorsDraft03[] = {
    // Simple message.
    {"lsemWwzlFoJzoidHCnVuxRiJpotTcYokJHKzmQ2FsA",
     {0x4D, 0x3A, 0x6C, 0xBA, 0xD8, 0x1D, 0x8E, 0x68, 0x8B, 0xE6, 0x76,
      0xA7, 0xFF, 0x60, 0xC7, 0xFE, 0x77, 0xE2, 0x6D, 0x37, 0xF6, 0x12,
      0x44, 0xE2, 0x25, 0xFE, 0xE1, 0xD8, 0xCF, 0x8A, 0xA8, 0x33},
     {0x62, 0x36, 0xAC, 0xCA, 0x74, 0xD4, 0x49, 0x49, 0x6B, 0x27, 0xB4, 0xF7,
      0xC1, 0xE5, 0x30, 0x9A},
     {0x1C, 0xA7, 0xFD, 0x98, 0x1A, 0xE4, 0xA7, 0x92, 0xE1, 0xB6, 0xA1, 0xE3,
      0x41, 0x63, 0x87, 0x76},
     4096,
     "Hello, world!"},
    // Simple message with 16 bytes of padding.
    {"VQB6Ds-q9xRqyM1tj_gksSgc78vCWEhphZ-NF1E7_yMfPuRRZlC_Xt9_2NsX3SU",
     {0x8B, 0x38, 0x8E, 0x22, 0xD5, 0xC4, 0xFD, 0x65, 0x8A, 0xBB, 0xD9,
      0x58, 0xBD, 0xF5, 0xFF, 0x79, 0xCF, 0x9D, 0xBD, 0x87, 0x16, 0x7E,
      0x93, 0x84, 0x20, 0x8E, 0x8D, 0x49, 0x41, 0x7D, 0x8E, 0x8F},
     {0x3E, 0x65, 0xC7, 0x1F, 0x75, 0x7A, 0x43, 0xC4, 0x78, 0x6C, 0x64, 0x99,
      0x49, 0xA0, 0xC4, 0xB2},
     {0x43, 0x4D, 0x30, 0x8E, 0xE4, 0x76, 0xB5, 0xD0, 0x87, 0xFC, 0x04, 0xD1,
      0x2E, 0x35, 0x75, 0x63},
     4096,
     "Hello, world!"},
    // Empty message.
    {"xU8a499UHB_-YSV4VOm-JZnT",
     {0x68, 0x72, 0x3D, 0x13, 0xE7, 0x50, 0xFA, 0x3E, 0xA0, 0x59, 0x33,
      0xF1, 0x73, 0xA8, 0xE8, 0xCD, 0x8D, 0xD4, 0x3C, 0xDC, 0xDE, 0x06,
      0x35, 0x5F, 0x51, 0xBB, 0xB2, 0x57, 0x97, 0x72, 0x9D, 0xFB},
     {0x84, 0xB2, 0x2A, 0xE7, 0xC6, 0xC0, 0xCE, 0x5F, 0xAD, 0x37, 0x06, 0x7F,
      0xD1, 0xFD, 0x10, 0x87},
     {0x9B, 0xC5, 0x8D, 0x5F, 0xD6, 0xD2, 0xA6, 0xBD, 0xAF, 0x4B, 0xD9, 0x60,
      0xC6, 0xB4, 0x50, 0x0F},
     4096,
     ""},
    // Message with an invalid record size.
    {"gfB-_edj7qEVokyVHpkDJN6FVKHnlWs1RCDw5bmrwQ",
     {0x5F, 0xE1, 0x7C, 0x4B, 0xFF, 0x04, 0xBF, 0x2C, 0x70, 0x67, 0xFA,
      0xF8, 0xB0, 0x07, 0x4F, 0xF6, 0x3C, 0x03, 0x6F, 0xBE, 0xA1, 0x1F,
      0x4B, 0x99, 0x25, 0x4F, 0xB9, 0x5F, 0xC4, 0x78, 0x76, 0xDE},
     {0x59, 0xAB, 0x45, 0xFC, 0x6A, 0xF5, 0xB3, 0xE0, 0xF5, 0x40, 0xD7, 0x98,
      0x0F, 0xF0, 0xA4, 0xCB},
     {0xDB, 0xA0, 0xF2, 0x91, 0x8D, 0x50, 0x42, 0xE0, 0x17, 0x68, 0x5B, 0x9B,
      0xF2, 0xA2, 0xC3, 0xF9},
     7,
     nullptr},
    // Message with four bytes of invalid, non-zero padding.
    {"2FJmrF95yVU8Q8cYQy9OoOwCb59ZoRlxazPE0T-MNOSMbr0",
     {0x6B, 0x82, 0x92, 0xD3, 0x71, 0x9A, 0x97, 0x76, 0x45, 0x11, 0x99,
      0x6D, 0xBF, 0x56, 0xCC, 0x81, 0x98, 0x56, 0x80, 0xF5, 0x78, 0x36,
      0xD6, 0x43, 0x95, 0x68, 0xDB, 0x0F, 0x23, 0x39, 0xF3, 0x6E},
     {0x02, 0x16, 0xDC, 0xC3, 0xDE, 0x2C, 0xB5, 0x08, 0x89, 0xDB, 0xD8, 0x18,
      0x68, 0x83, 0x1C, 0xDB},
     {0xB7, 0x85, 0x5D, 0x8E, 0x84, 0xC3, 0x2D, 0x61, 0x9B, 0x78, 0x3B, 0x60,
      0x0E, 0x70, 0x84, 0xF3},
     4096,
     nullptr},
    // Message with multiple (2) records.
    {"reI6sW6y67FI8Kxk-x9GNwiu77His_f5GioDBiKS7IzjDQ",
     {0xC6, 0x16, 0x6F, 0xAF, 0xE1, 0xB6, 0x8F, 0x2B, 0x0F, 0x67, 0x5A,
      0xC7, 0xAC, 0x7E, 0xF6, 0x7C, 0x33, 0xA2, 0xA1, 0x11, 0xB0, 0xB0,
      0xAB, 0xAC, 0x37, 0x61, 0xF4, 0xCB, 0x98, 0xFF, 0x00, 0x51},
     {0xAE, 0xDA, 0x86, 0xDF, 0x6B, 0x03, 0x88, 0xDE, 0x90, 0xBB, 0xB7, 0xA0,
      0x78, 0x91, 0x3A, 0x36},
     {0x4C, 0x4E, 0x2A, 0x8D, 0x88, 0x82, 0xCF, 0xC2, 0xF9, 0x8A, 0xFD, 0x31,
      0xF8, 0xD1, 0xF6, 0xB5},
     8,
     nullptr}};

const TestVector kDecryptionTestVectorsDraft08[] = {
    // Simple message.
    {"baIDPDv-Do_x1RVtlFDex2uCvd3Ugrv-gJG3sWeg",
     {0x4D, 0x3A, 0x6C, 0xBA, 0xD8, 0x1D, 0x8E, 0x68, 0x8B, 0xE6, 0x76,
      0xA7, 0xFF, 0x60, 0xC7, 0xFE, 0x77, 0xE2, 0x6D, 0x37, 0xF6, 0x12,
      0x44, 0xE2, 0x25, 0xFE, 0xE1, 0xD8, 0xCF, 0x8A, 0xA8, 0x33},
     {0x62, 0x36, 0xAC, 0xCA, 0x74, 0xD4, 0x49, 0x49, 0x6B, 0x27, 0xB4, 0xF7,
      0xC1, 0xE5, 0x30, 0x9A},
     {0x1C, 0xA7, 0xFD, 0x98, 0x1A, 0xE4, 0xA7, 0x92, 0xE1, 0xB6, 0xA1, 0xE3,
      0x41, 0x63, 0x87, 0x76},
     4096,
     "Hello, world!"},
    // Simple message with 16 bytes of padding.
    {"6Zq7GKQ7zRxeOWoYR71Nx7xJzCZUUNhz6bhV1-ZIg6dVra0x1uWXms5gHp6F6A",
     {0x8B, 0x38, 0x8E, 0x22, 0xD5, 0xC4, 0xFD, 0x65, 0x8A, 0xBB, 0xD9,
      0x58, 0xBD, 0xF5, 0xFF, 0x79, 0xCF, 0x9D, 0xBD, 0x87, 0x16, 0x7E,
      0x93, 0x84, 0x20, 0x8E, 0x8D, 0x49, 0x41, 0x7D, 0x8E, 0x8F},
     {0x3E, 0x65, 0xC7, 0x1F, 0x75, 0x7A, 0x43, 0xC4, 0x78, 0x6C, 0x64, 0x99,
      0x49, 0xA0, 0xC4, 0xB2},
     {0x43, 0x4D, 0x30, 0x8E, 0xE4, 0x76, 0xB5, 0xD0, 0x87, 0xFC, 0x04, 0xD1,
      0x2E, 0x35, 0x75, 0x63},
     4096,
     "Hello, world!"},
    // Empty message.
    {"bHU7ponA7WAGB0onUybG9nQ",
     {0x68, 0x72, 0x3D, 0x13, 0xE7, 0x50, 0xFA, 0x3E, 0xA0, 0x59, 0x33,
      0xF1, 0x73, 0xA8, 0xE8, 0xCD, 0x8D, 0xD4, 0x3C, 0xDC, 0xDE, 0x06,
      0x35, 0x5F, 0x51, 0xBB, 0xB2, 0x57, 0x97, 0x72, 0x9D, 0xFB},
     {0x84, 0xB2, 0x2A, 0xE7, 0xC6, 0xC0, 0xCE, 0x5F, 0xAD, 0x37, 0x06, 0x7F,
      0xD1, 0xFD, 0x10, 0x87},
     {0x9B, 0xC5, 0x8D, 0x5F, 0xD6, 0xD2, 0xA6, 0xBD, 0xAF, 0x4B, 0xD9, 0x60,
      0xC6, 0xB4, 0x50, 0x0F},
     4096,
     ""}};

// Computes the shared secret between the sender and the receiver. The sender
// must have a ASN.1-encoded PKCS #8 EncryptedPrivateKeyInfo block, whereas
// the receiver must have a public key in uncompressed EC point format.
bool ComputeSharedP256SecretFromPrivateKeyStr(std::string_view private_key,
                                              std::string_view peer_public_key,
                                              std::string* out_shared_secret) {
  DCHECK(out_shared_secret);
  std::unique_ptr<crypto::ECPrivateKey> local_key(
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(std::vector<uint8_t>(
          private_key.data(), private_key.data() + private_key.size())));
  if (!local_key) {
    DLOG(ERROR) << "Unable to create the local key";
    return false;
  }

  return ComputeSharedP256Secret(*local_key, peer_public_key,
                                 out_shared_secret);
}

void ComputeSharedSecret(std::string_view encoded_sender_private_key,
                         std::string_view encoded_receiver_public_key,
                         std::string* shared_secret) {
  std::string sender_private_key, receiver_public_key;
  ASSERT_TRUE(base::Base64UrlDecode(
      encoded_sender_private_key,
      base::Base64UrlDecodePolicy::IGNORE_PADDING, &sender_private_key));
  ASSERT_TRUE(base::Base64UrlDecode(
      encoded_receiver_public_key,
      base::Base64UrlDecodePolicy::IGNORE_PADDING, &receiver_public_key));

  ASSERT_TRUE(ComputeSharedP256SecretFromPrivateKeyStr(
      sender_private_key, receiver_public_key,
      shared_secret));
}

}  // namespace

class GCMMessageCryptographerTestBase : public ::testing::Test {
 public:
  void SetUp() override {
    recipient_public_key_.assign(
        kCommonRecipientPublicKey,
        kCommonRecipientPublicKey + std::size(kCommonRecipientPublicKey));
    sender_public_key_.assign(
        kCommonSenderPublicKey,
        kCommonSenderPublicKey + std::size(kCommonSenderPublicKey));

    std::string recipient_private_key(
        kCommonRecipientPrivateKey,
        kCommonRecipientPrivateKey + std::size(kCommonRecipientPrivateKey));
    std::vector<uint8_t> recipient_private_key_vec(
      recipient_private_key.begin(), recipient_private_key.end());
    std::unique_ptr<crypto::ECPrivateKey> recipient_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(recipient_private_key_vec);
    ASSERT_TRUE(recipient_key);
    ASSERT_TRUE(ComputeSharedP256Secret(
        *recipient_key, sender_public_key_, &ecdh_shared_secret_));

    auth_secret_.assign(kCommonAuthSecret,
                        kCommonAuthSecret + std::size(kCommonAuthSecret));
  }

 protected:
  // Public keys of the recipient and sender as uncompressed P-256 EC points.
  std::string recipient_public_key_;
  std::string sender_public_key_;

  // Shared secret to use in transformations. Associated with the keys above.
  std::string ecdh_shared_secret_;

  // Authentication secret to use in tests where no specific value is expected.
  std::string auth_secret_;
};

class GCMMessageCryptographerTest
    : public GCMMessageCryptographerTestBase,
      public testing::WithParamInterface<GCMMessageCryptographer::Version> {
 public:
  void SetUp() override {
    GCMMessageCryptographerTestBase::SetUp();

    cryptographer_ = std::make_unique<GCMMessageCryptographer>(GetParam());
  }

 protected:
  // Generates a cryptographically secure random salt of 16-octets in size, the
  // required length as expected by the HKDF.
  std::string GenerateRandomSalt() {
    std::string salt(kSaltSize, '\0');
    crypto::RandBytes(base::as_writable_byte_span(salt));
    return salt;
  }

  // The GCMMessageCryptographer instance to use for the tests.
  std::unique_ptr<GCMMessageCryptographer> cryptographer_;
};

TEST_P(GCMMessageCryptographerTest, RoundTrip) {
  const std::string salt = GenerateRandomSalt();

  size_t record_size = 0;

  std::string ciphertext, plaintext;
  ASSERT_TRUE(cryptographer_->Encrypt(
      recipient_public_key_, sender_public_key_, ecdh_shared_secret_,
      auth_secret_, salt, kExamplePlaintext, &record_size, &ciphertext));

  EXPECT_GT(record_size, ciphertext.size() - 16);
  EXPECT_GT(ciphertext.size(), 0u);

  ASSERT_TRUE(cryptographer_->Decrypt(recipient_public_key_, sender_public_key_,
                                      ecdh_shared_secret_, auth_secret_, salt,
                                      ciphertext, record_size, &plaintext));

  EXPECT_EQ(kExamplePlaintext, plaintext);
}

TEST_P(GCMMessageCryptographerTest, RoundTripEmptyMessage) {
  const std::string salt = GenerateRandomSalt();
  const std::string message;

  size_t record_size = 0;

  std::string ciphertext, plaintext;
  ASSERT_TRUE(cryptographer_->Encrypt(recipient_public_key_, sender_public_key_,
                                      ecdh_shared_secret_, auth_secret_, salt,
                                      message, &record_size, &ciphertext));

  EXPECT_GT(record_size, ciphertext.size() - 16);
  EXPECT_GT(ciphertext.size(), 0u);

  ASSERT_TRUE(cryptographer_->Decrypt(recipient_public_key_, sender_public_key_,
                                      ecdh_shared_secret_, auth_secret_, salt,
                                      ciphertext, record_size, &plaintext));

  EXPECT_EQ(message, plaintext);
}

TEST_P(GCMMessageCryptographerTest, InvalidRecordSize) {
  const std::string salt = GenerateRandomSalt();

  size_t record_size = 0;

  std::string ciphertext, plaintext;
  ASSERT_TRUE(cryptographer_->Encrypt(
      recipient_public_key_, sender_public_key_, ecdh_shared_secret_,
      auth_secret_, salt, kExamplePlaintext, &record_size, &ciphertext));

  EXPECT_GT(record_size, ciphertext.size() - 16);

  EXPECT_FALSE(cryptographer_->Decrypt(
      recipient_public_key_, sender_public_key_, ecdh_shared_secret_,
      auth_secret_, salt, ciphertext, 0 /* record_size */, &plaintext));

  EXPECT_FALSE(cryptographer_->Decrypt(
      recipient_public_key_, sender_public_key_, ecdh_shared_secret_,
      auth_secret_, salt, ciphertext, ciphertext.size() - 17, &plaintext));

  EXPECT_TRUE(cryptographer_->Decrypt(
      recipient_public_key_, sender_public_key_, ecdh_shared_secret_,
      auth_secret_, salt, ciphertext, ciphertext.size() - 16, &plaintext));
}

TEST_P(GCMMessageCryptographerTest, InvalidRecordPadding) {
  std::string message;
  switch (GetParam()) {
    case GCMMessageCryptographer::Version::DRAFT_03:
      message.append(sizeof(uint8_t), '\00');  // padding length octets
      message.append(sizeof(uint8_t), '\01');

      message.append(sizeof(uint8_t), '\00');  // padding octet
      message.append(kExamplePlaintext);
      break;
    case GCMMessageCryptographer::Version::DRAFT_08:
      message.append(kExamplePlaintext);
      message.append(sizeof(uint8_t), '\x02');  // padding delimiter octet
      message.append(sizeof(uint8_t), '\x00');  // padding octet
      break;
  }

  const std::string salt = GenerateRandomSalt();

  const std::string prk =
      cryptographer_->encryption_scheme_->DerivePseudoRandomKey(
          recipient_public_key_, sender_public_key_, ecdh_shared_secret_,
          auth_secret_);
  const std::string content_encryption_key =
      cryptographer_->DeriveContentEncryptionKey(recipient_public_key_,
                                                 sender_public_key_, prk, salt);
  const std::string nonce = cryptographer_->DeriveNonce(
      recipient_public_key_, sender_public_key_, prk, salt);

  ASSERT_GT(message.size(), 2u);
  const size_t record_size = message.size() + 1;

  std::string ciphertext, plaintext;
  ASSERT_TRUE(cryptographer_->TransformRecord(
      GCMMessageCryptographer::Direction::ENCRYPT, message,
      content_encryption_key, nonce, &ciphertext));

  ASSERT_TRUE(cryptographer_->Decrypt(recipient_public_key_, sender_public_key_,
                                      ecdh_shared_secret_, auth_secret_, salt,
                                      ciphertext, record_size, &plaintext));

  // Note that GCMMessageCryptographer::Decrypt removes the padding.
  EXPECT_EQ(kExamplePlaintext, plaintext);

  // Now run the same steps again, but have invalid padding length indicators.
  // (Only applicable to draft-ietf-webpush-encryption-03.)
  if (GetParam() == GCMMessageCryptographer::Version::DRAFT_03) {
    // Padding that will spill over in the payload.
    {
      message[1] = 4;

      ASSERT_TRUE(cryptographer_->TransformRecord(
          GCMMessageCryptographer::Direction::ENCRYPT, message,
          content_encryption_key, nonce, &ciphertext));

      ASSERT_FALSE(cryptographer_->Decrypt(
          recipient_public_key_, sender_public_key_, ecdh_shared_secret_,
          auth_secret_, salt, ciphertext, record_size, &plaintext));
    }

    // More padding octets than the length of the message.
    {
      message[1] = 64;

      ASSERT_TRUE(cryptographer_->TransformRecord(
          GCMMessageCryptographer::Direction::ENCRYPT, message,
          content_encryption_key, nonce, &ciphertext));

      ASSERT_FALSE(cryptographer_->Decrypt(
          recipient_public_key_, sender_public_key_, ecdh_shared_secret_,
          auth_secret_, salt, ciphertext, record_size, &plaintext));
    }

    // Correct the |message| to be valid again. (A single byte of padding.)
    message[1] = 1;
  }

  // Run tests for a missing delimiter in the record.
  // (Only applicable to draft-ietf-webpush-encryption-03.)
  if (GetParam() == GCMMessageCryptographer::Version::DRAFT_08) {
    message[message.size() - 2] = 0x00;

    ASSERT_TRUE(cryptographer_->TransformRecord(
        GCMMessageCryptographer::Direction::ENCRYPT, message,
        content_encryption_key, nonce, &ciphertext));

    ASSERT_FALSE(cryptographer_->Decrypt(
        recipient_public_key_, sender_public_key_, ecdh_shared_secret_,
        auth_secret_, salt, ciphertext, record_size, &plaintext));

    // Correct the |message| to be valid again. (Proper padding delimiter.)
    message[message.size() - 2] = 0x02;
  }

  // Finally run a test to make sure that we validate that all padding bytes are
  // set to zeros. The position of the padding byte depends on the version.
  switch (GetParam()) {
    case GCMMessageCryptographer::Version::DRAFT_03:
      message[2] = 0x13;
      break;
    case GCMMessageCryptographer::Version::DRAFT_08:
      message[message.size() - 1] = 0x13;
      break;
  }

  ASSERT_TRUE(cryptographer_->TransformRecord(
      GCMMessageCryptographer::Direction::ENCRYPT, message,
      content_encryption_key, nonce, &ciphertext));

  ASSERT_FALSE(cryptographer_->Decrypt(
      recipient_public_key_, sender_public_key_, ecdh_shared_secret_,
      auth_secret_, salt, ciphertext, record_size, &plaintext));
}

TEST_P(GCMMessageCryptographerTest, AuthSecretAffectsPRK) {
  std::string first_auth_secret(kAuthSecretSize, '\0');
  crypto::RandBytes(base::as_writable_byte_span(first_auth_secret));
  std::string second_auth_secret(kAuthSecretSize, '\0');
  crypto::RandBytes(base::as_writable_byte_span(second_auth_secret));

  ASSERT_NE(cryptographer_->encryption_scheme_->DerivePseudoRandomKey(
                recipient_public_key_, sender_public_key_, ecdh_shared_secret_,
                first_auth_secret),
            cryptographer_->encryption_scheme_->DerivePseudoRandomKey(
                recipient_public_key_, sender_public_key_, ecdh_shared_secret_,
                second_auth_secret));

  std::string salt = GenerateRandomSalt();

  // Verify that the IKM actually gets used by the transformations.
  size_t first_record_size, second_record_size;
  std::string first_ciphertext, second_ciphertext;

  ASSERT_TRUE(cryptographer_->Encrypt(recipient_public_key_, sender_public_key_,
                                      ecdh_shared_secret_, first_auth_secret,
                                      salt, kExamplePlaintext,
                                      &first_record_size, &first_ciphertext));

  ASSERT_TRUE(cryptographer_->Encrypt(recipient_public_key_, sender_public_key_,
                                      ecdh_shared_secret_, second_auth_secret,
                                      salt, kExamplePlaintext,
                                      &second_record_size, &second_ciphertext));

  // If the ciphertexts differ despite the same key and salt, it got used.
  ASSERT_NE(first_ciphertext, second_ciphertext);
  EXPECT_EQ(first_record_size, second_record_size);

  // Verify that the different ciphertexts can also be translated back to the
  // plaintext content. This will fail if the auth secret isn't considered.
  std::string first_plaintext, second_plaintext;

  ASSERT_TRUE(cryptographer_->Decrypt(recipient_public_key_, sender_public_key_,
                                      ecdh_shared_secret_, first_auth_secret,
                                      salt, first_ciphertext, first_record_size,
                                      &first_plaintext));

  ASSERT_TRUE(cryptographer_->Decrypt(recipient_public_key_, sender_public_key_,
                                      ecdh_shared_secret_, second_auth_secret,
                                      salt, second_ciphertext,
                                      second_record_size, &second_plaintext));

  EXPECT_EQ(kExamplePlaintext, first_plaintext);
  EXPECT_EQ(kExamplePlaintext, second_plaintext);
}

INSTANTIATE_TEST_SUITE_P(
    GCMMessageCryptographerTestBase,
    GCMMessageCryptographerTest,
    ::testing::Values(GCMMessageCryptographer::Version::DRAFT_03,
                      GCMMessageCryptographer::Version::DRAFT_08));

class GCMMessageCryptographerTestVectorTest
    : public GCMMessageCryptographerTestBase {};

TEST_F(GCMMessageCryptographerTestVectorTest, EncryptionVectorsDraft03) {
  GCMMessageCryptographer cryptographer(
      GCMMessageCryptographer::Version::DRAFT_03);

  std::string ecdh_shared_secret, auth_secret, salt, ciphertext, output;
  size_t record_size = 0;

  for (size_t i = 0; i < std::size(kEncryptionTestVectorsDraft03); ++i) {
    SCOPED_TRACE(i);

    ecdh_shared_secret.assign(
        kEncryptionTestVectorsDraft03[i].ecdh_shared_secret,
        kEncryptionTestVectorsDraft03[i].ecdh_shared_secret +
            kEcdhSharedSecretSize);

    auth_secret.assign(
        kEncryptionTestVectorsDraft03[i].auth_secret,
        kEncryptionTestVectorsDraft03[i].auth_secret + kAuthSecretSize);

    salt.assign(kEncryptionTestVectorsDraft03[i].salt,
                kEncryptionTestVectorsDraft03[i].salt + kSaltSize);

    ASSERT_TRUE(cryptographer.Encrypt(recipient_public_key_, sender_public_key_,
                                      ecdh_shared_secret, auth_secret, salt,
                                      kEncryptionTestVectorsDraft03[i].input,
                                      &record_size, &ciphertext));

    base::Base64UrlEncode(ciphertext, base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &output);

    EXPECT_EQ(kEncryptionTestVectorsDraft03[i].record_size, record_size);
    EXPECT_EQ(kEncryptionTestVectorsDraft03[i].output, output);
  }
}

TEST_F(GCMMessageCryptographerTestVectorTest, DecryptionVectorsDraft03) {
  GCMMessageCryptographer cryptographer(
      GCMMessageCryptographer::Version::DRAFT_03);

  std::string input, ecdh_shared_secret, auth_secret, salt, plaintext;
  for (size_t i = 0; i < std::size(kDecryptionTestVectorsDraft03); ++i) {
    SCOPED_TRACE(i);

    ASSERT_TRUE(base::Base64UrlDecode(
        kDecryptionTestVectorsDraft03[i].input,
        base::Base64UrlDecodePolicy::IGNORE_PADDING, &input));

    ecdh_shared_secret.assign(
        kDecryptionTestVectorsDraft03[i].ecdh_shared_secret,
        kDecryptionTestVectorsDraft03[i].ecdh_shared_secret +
            kEcdhSharedSecretSize);

    auth_secret.assign(
        kDecryptionTestVectorsDraft03[i].auth_secret,
        kDecryptionTestVectorsDraft03[i].auth_secret + kAuthSecretSize);

    salt.assign(kDecryptionTestVectorsDraft03[i].salt,
                kDecryptionTestVectorsDraft03[i].salt + kSaltSize);

    const bool has_output = kDecryptionTestVectorsDraft03[i].output;
    const bool result = cryptographer.Decrypt(
        recipient_public_key_, sender_public_key_, ecdh_shared_secret,
        auth_secret, salt, input, kDecryptionTestVectorsDraft03[i].record_size,
        &plaintext);

    if (!has_output) {
      EXPECT_FALSE(result);
      continue;
    }

    EXPECT_TRUE(result);
    EXPECT_EQ(kDecryptionTestVectorsDraft03[i].output, plaintext);
  }
}

TEST_F(GCMMessageCryptographerTestVectorTest, EncryptionVectorsDraft08) {
  GCMMessageCryptographer cryptographer(
      GCMMessageCryptographer::Version::DRAFT_08);

  std::string ecdh_shared_secret, auth_secret, salt, ciphertext, output;
  size_t record_size = 0;

  for (size_t i = 0; i < std::size(kEncryptionTestVectorsDraft08); ++i) {
    SCOPED_TRACE(i);

    ecdh_shared_secret.assign(
        kEncryptionTestVectorsDraft08[i].ecdh_shared_secret,
        kEncryptionTestVectorsDraft08[i].ecdh_shared_secret +
            kEcdhSharedSecretSize);

    auth_secret.assign(
        kEncryptionTestVectorsDraft08[i].auth_secret,
        kEncryptionTestVectorsDraft08[i].auth_secret + kAuthSecretSize);

    salt.assign(kEncryptionTestVectorsDraft08[i].salt,
                kEncryptionTestVectorsDraft08[i].salt + kSaltSize);

    ASSERT_TRUE(cryptographer.Encrypt(recipient_public_key_, sender_public_key_,
                                      ecdh_shared_secret, auth_secret, salt,
                                      kEncryptionTestVectorsDraft08[i].input,
                                      &record_size, &ciphertext));

    base::Base64UrlEncode(ciphertext, base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &output);

    EXPECT_EQ(kEncryptionTestVectorsDraft08[i].record_size, record_size);
    EXPECT_EQ(kEncryptionTestVectorsDraft08[i].output, output);
  }
}

TEST_F(GCMMessageCryptographerTestVectorTest, DecryptionVectorsDraft08) {
  GCMMessageCryptographer cryptographer(
      GCMMessageCryptographer::Version::DRAFT_08);

  std::string input, ecdh_shared_secret, auth_secret, salt, plaintext;

  for (size_t i = 0; i < std::size(kDecryptionTestVectorsDraft08); ++i) {
    SCOPED_TRACE(i);

    ASSERT_TRUE(base::Base64UrlDecode(
        kDecryptionTestVectorsDraft08[i].input,
        base::Base64UrlDecodePolicy::IGNORE_PADDING, &input));

    ecdh_shared_secret.assign(
        kDecryptionTestVectorsDraft08[i].ecdh_shared_secret,
        kDecryptionTestVectorsDraft08[i].ecdh_shared_secret +
            kEcdhSharedSecretSize);

    auth_secret.assign(
        kDecryptionTestVectorsDraft08[i].auth_secret,
        kDecryptionTestVectorsDraft08[i].auth_secret + kAuthSecretSize);

    salt.assign(kDecryptionTestVectorsDraft08[i].salt,
                kDecryptionTestVectorsDraft08[i].salt + kSaltSize);

    const bool has_output = kDecryptionTestVectorsDraft08[i].output;
    const bool result = cryptographer.Decrypt(
        recipient_public_key_, sender_public_key_, ecdh_shared_secret,
        auth_secret, salt, input, kDecryptionTestVectorsDraft08[i].record_size,
        &plaintext);

    if (!has_output) {
      EXPECT_FALSE(result);
      continue;
    }

    EXPECT_TRUE(result);
    EXPECT_EQ(kDecryptionTestVectorsDraft08[i].output, plaintext);
  }
}

class GCMMessageCryptographerReferenceTest : public ::testing::Test {};

// Reference test included for the Version::DRAFT_03 implementation.
// https://tools.ietf.org/html/draft-ietf-webpush-encryption-03
// https://tools.ietf.org/html/draft-ietf-httpbis-encryption-encoding-02
TEST_F(GCMMessageCryptographerReferenceTest, ReferenceDraft03) {
  // The 16-byte salt unique to the message.
  const char kSalt[] = "lngarbyKfMoi9Z75xYXmkg";

  // The 16-byte prearranged secret between the sender and receiver.
  const char kAuthSecret[] = "R29vIGdvbyBnJyBqb29iIQ";

  // The keying material used by the sender to encrypt the |kCiphertext|.
  const char kSenderPrivate[] =
      "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgnCScek-QpEjmOOlT-rQ38nZz"
      "vdPlqa00Zy0i6m2OJvahRANCAATaEQ22_OCRpvIOWeQhcbq0qrF1iddSLX1xFmFSxPOWOwmJ"
      "A417CBHOGqsWGkNRvAapFwiegz6Q61rXVo_5roB1";
  const char kSenderPublicKeyUncompressed[] =
      "BNoRDbb84JGm8g5Z5CFxurSqsXWJ11ItfXEWYVLE85Y7CYkDjXsIEc4aqxYaQ1G8BqkXCJ6D"
      "PpDrWtdWj_mugHU";

  // The keying material used by the recipient to decrypt the |kCiphertext|.
  const char kRecipientPrivate[] =
      "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg9FWl15_QUQAWDaD3k3l50ZBZ"
      "QJ4au27F1V4F0uLSD_OhRANCAAQhJAY8y_GdwvqItkO6BObdjafqe6LIxi4Pd6lD9ML6kU9t"
      "RBFsn9HEA0HGpEDKs-IUCmDkN4pdpzWXLeB4AFEF";
  const char kRecipientPublicKeyUncompressed[] =
      "BCEkBjzL8Z3C-oi2Q7oE5t2Np-p7osjGLg93qUP0wvqRT21EEWyf0cQDQcakQMqz4hQKYOQ3"
      "il2nNZct4HgAUQU";

  // The ciphertext and associated plaintext of the message.
  const char kCiphertext[] = "6nqAQUME8hNqw5J3kl8cpVVJylXKYqZOeseZG8UueKpA";
  const char kPlaintext[] = "I am the walrus";

  std::string sender_shared_secret, receiver_shared_secret;

  // Compute the shared secrets between the sender and receiver's keys.
  ASSERT_NO_FATAL_FAILURE(ComputeSharedSecret(
      kSenderPrivate, kRecipientPublicKeyUncompressed, &sender_shared_secret));
  ASSERT_NO_FATAL_FAILURE(ComputeSharedSecret(kRecipientPrivate,
                                              kSenderPublicKeyUncompressed,
                                              &receiver_shared_secret));

  ASSERT_GT(sender_shared_secret.size(), 0u);
  ASSERT_EQ(sender_shared_secret, receiver_shared_secret);

  // Decode the public keys of both parties, the auth secret and the salt.
  std::string recipient_public_key, sender_public_key, auth_secret, salt;
  ASSERT_TRUE(base::Base64UrlDecode(kRecipientPublicKeyUncompressed,
                                    base::Base64UrlDecodePolicy::IGNORE_PADDING,
                                    &recipient_public_key));
  ASSERT_TRUE(base::Base64UrlDecode(kSenderPublicKeyUncompressed,
                                    base::Base64UrlDecodePolicy::IGNORE_PADDING,
                                    &sender_public_key));
  ASSERT_TRUE(base::Base64UrlDecode(
      kAuthSecret, base::Base64UrlDecodePolicy::IGNORE_PADDING, &auth_secret));
  ASSERT_TRUE(base::Base64UrlDecode(
        kSalt, base::Base64UrlDecodePolicy::IGNORE_PADDING, &salt));

  std::string encoded_ciphertext, ciphertext, plaintext;
  size_t record_size = 0;

  // Now verify that encrypting a message with the given information yields the
  // expected ciphertext given the defined input.
  GCMMessageCryptographer cryptographer(
      GCMMessageCryptographer::Version::DRAFT_03);

  ASSERT_TRUE(cryptographer.Encrypt(recipient_public_key, sender_public_key,
                                    sender_shared_secret, auth_secret, salt,
                                    kPlaintext, &record_size, &ciphertext));

  base::Base64UrlEncode(ciphertext, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_ciphertext);
  ASSERT_EQ(kCiphertext, encoded_ciphertext);

  // And verify that decrypting the message yields the plaintext again.
  ASSERT_TRUE(cryptographer.Decrypt(recipient_public_key, sender_public_key,
                                    sender_shared_secret, auth_secret, salt,
                                    ciphertext, record_size, &plaintext));

  ASSERT_EQ(kPlaintext, plaintext);
}

// Reference test included for the Version::DRAFT_08 implementation.
// https://tools.ietf.org/html/draft-ietf-webpush-encryption-08
// https://tools.ietf.org/html/draft-ietf-httpbis-encryption-encoding-07
TEST_F(GCMMessageCryptographerReferenceTest, ReferenceDraft08) {
  // The 16-byte prearranged secret between the sender and receiver.
  const char kAuthSecret[] = "BTBZMqHH6r4Tts7J_aSIgg";

  // The keying material used by the sender to encrypt the |kCiphertext|.
  const char kSenderPrivate[] =
      "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgyfWPiYE-n46HLnH0KqZOF1fJ"
      "JU3MYrct3AELtAQ-oRyhRANCAAT-M_SrDepxkU21WCP3O1SUj0EwbZIHMtu5pZpTKGSCIA5Z"
      "ent7wmC6HCJ5mFgJkuk5cwAvMBKiiujwa7t45ewP";

  // The keying material used by the recipient to decrypt the |kCiphertext|.
const char kRecipientPrivate[] =
      "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgq1dXpw3UpT5VOmu_cf_v6ih0"
      "7Aems3njxI-JWgLcM96hRANCAAQlcbK-zf3jYFUarx7Q9M02bBHOvlVfiby3sYalMzkXMWjs"
      "4uvgGFl70wR5uG48j47O1XfKWRh-kkaZDbaCAIsO";
  const char kRecipientPublicKeyUncompressed[] =
      "BCVxsr7N_eNgVRqvHtD0zTZsEc6-VV-JvLexhqUzORcxaOzi6-AYWXvTBHm4bjyPjs7Vd8pZ"
      "GH6SRpkNtoIAiw4";

  // The plain text of the message, as well as the encrypted reference message.
  const char kPlaintext[] = "When I grow up, I want to be a watermelon";
  const char kReferenceMessage[] =
      "DGv6ra1nlYgDCS1FRnbzlwAAEABBBP4z9KsN6nGRTbVYI_"
      "c7VJSPQTBtkgcy27mlmlMoZIIgDll6e3vCYLocInmYWAmS6TlzAC8wEqKK6PBru3jl7A_"
      "yl95bQpu6cVPTpK4Mqgkf1CXztLVBSt2Ks3oZwbuwXPXLWyouBWLVWGNWQexSgSxsj_"
      "Qulcy4a-fN";

  std::string message;
  ASSERT_TRUE(base::Base64UrlDecode(kReferenceMessage,
                                    base::Base64UrlDecodePolicy::IGNORE_PADDING,
                                    &message));

  MessagePayloadParser message_parser(message);
  ASSERT_TRUE(message_parser.IsValid());

  std::string_view salt = message_parser.salt();
  uint32_t record_size = message_parser.record_size();
  std::string_view sender_public_key = message_parser.public_key();
  std::string_view ciphertext = message_parser.ciphertext();

  std::string sender_shared_secret, receiver_shared_secret;

  // Compute the shared secrets between the sender and receiver's keys.
  ASSERT_NO_FATAL_FAILURE(ComputeSharedSecret(
      kSenderPrivate, kRecipientPublicKeyUncompressed, &sender_shared_secret));

  // Compute the shared secret based on the sender's public key, which isn't a
  // constant but instead is included in the message's binary header.
  std::string recipient_private_key;
  ASSERT_TRUE(base::Base64UrlDecode(kRecipientPrivate,
                                    base::Base64UrlDecodePolicy::IGNORE_PADDING,
                                    &recipient_private_key));
  ASSERT_NO_FATAL_FAILURE(ComputeSharedP256SecretFromPrivateKeyStr(
      recipient_private_key, sender_public_key,
      &receiver_shared_secret));

  ASSERT_GT(sender_shared_secret.size(), 0u);
  ASSERT_EQ(sender_shared_secret, receiver_shared_secret);

  // Decode the public keys of both parties and the auth secret.
  std::string recipient_public_key, auth_secret;
  ASSERT_TRUE(base::Base64UrlDecode(kRecipientPublicKeyUncompressed,
                                    base::Base64UrlDecodePolicy::IGNORE_PADDING,
                                    &recipient_public_key));
  ASSERT_TRUE(base::Base64UrlDecode(
      kAuthSecret, base::Base64UrlDecodePolicy::IGNORE_PADDING, &auth_secret));

  // Attempt to decrypt the message using a GCMMessageCryptographer for this
  // version of the draft, and then re-encrypt it agian to make sure it matches.
  GCMMessageCryptographer cryptographer(
      GCMMessageCryptographer::Version::DRAFT_08);

  std::string plaintext;

  ASSERT_TRUE(cryptographer.Decrypt(recipient_public_key, sender_public_key,
                                    sender_shared_secret, auth_secret, salt,
                                    ciphertext, record_size, &plaintext));
  ASSERT_EQ(kPlaintext, plaintext);

  size_t record_size2;
  std::string ciphertext2;

  ASSERT_TRUE(cryptographer.Encrypt(recipient_public_key, sender_public_key,
                                    sender_shared_secret, auth_secret, salt,
                                    kPlaintext, &record_size2, &ciphertext2));

  EXPECT_GE(record_size2, record_size);
  EXPECT_EQ(ciphertext2, ciphertext);
}

}  // namespace gcm
