// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/crx_file/crx_verifier.h"

#include <algorithm>
#include <climits>
#include <cstring>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/base64.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "components/crx_file/crx3.pb.h"
#include "components/crx_file/crx_file.h"
#include "components/crx_file/id_util.h"
#include "crypto/secure_hash.h"
#include "crypto/secure_util.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"

namespace crx_file {

namespace {

// The SHA256 hash of the DER SPKI "ecdsa_2017_public" Crx3 key.
constexpr uint8_t kPublisherKeyHash[] = {
    0x61, 0xf7, 0xf2, 0xa6, 0xbf, 0xcf, 0x74, 0xcd, 0x0b, 0xc1, 0xfe,
    0x24, 0x97, 0xcc, 0x9b, 0x04, 0x25, 0x4c, 0x65, 0x8f, 0x79, 0xf2,
    0x14, 0x53, 0x92, 0x86, 0x7e, 0xa8, 0x36, 0x63, 0x67, 0xcf};

// The SHA256 hash of the DER SPKI "ecdsa_2017_public" Crx3 test key.
constexpr uint8_t kPublisherTestKeyHash[] = {
    0x6c, 0x46, 0x41, 0x3b, 0x00, 0xd0, 0xfa, 0x0e, 0x72, 0xc8, 0xd2,
    0x5f, 0x64, 0xf3, 0xa6, 0x17, 0x03, 0x0d, 0xde, 0x21, 0x61, 0xbe,
    0xb7, 0x95, 0x91, 0x95, 0x83, 0x68, 0x12, 0xe9, 0x78, 0x1e};

constexpr uint8_t kEocd[] = {'P', 'K', 0x05, 0x06};
constexpr uint8_t kEocd64[] = {'P', 'K', 0x06, 0x07};

using VerifierCollection =
    std::vector<std::unique_ptr<crypto::SignatureVerifier>>;
using RepeatedProof = google::protobuf::RepeatedPtrField<AsymmetricKeyProof>;

int ReadAndHashBuffer(uint8_t* buffer,
                      int length,
                      base::File* file,
                      crypto::SecureHash* hash) {
  static_assert(sizeof(char) == sizeof(uint8_t), "Unsupported char size.");
  int read = file->ReadAtCurrentPos(reinterpret_cast<char*>(buffer), length);
  if (read > 0) {
    hash->Update(buffer, read);
  }
  return read;
}

// Returns UINT32_MAX in the case of an unexpected EOF or read error, else
// returns the read uint32.
uint32_t ReadAndHashLittleEndianUInt32(base::File* file,
                                       crypto::SecureHash* hash) {
  uint8_t buffer[4] = {};
  if (ReadAndHashBuffer(buffer, 4, file, hash) != 4) {
    return UINT32_MAX;
  }
  return buffer[3] << 24 | buffer[2] << 16 | buffer[1] << 8 | buffer[0];
}

// Read to the end of the file, updating the hash and all verifiers.
bool ReadHashAndVerifyArchive(base::File* file,
                              crypto::SecureHash* hash,
                              const VerifierCollection& verifiers) {
  uint8_t buffer[1 << 12] = {};
  size_t len = 0;
  while ((len = ReadAndHashBuffer(buffer, std::size(buffer), file, hash)) > 0) {
    for (auto& verifier : verifiers) {
      verifier->VerifyUpdate(base::make_span(buffer, len));
    }
  }
  for (auto& verifier : verifiers) {
    if (!verifier->VerifyFinal()) {
      return false;
    }
  }
  return len == 0;
}

// The remaining contents of a Crx3 file are [header-size][header][archive].
// [header] is an encoded protocol buffer and contains both a signed and
// unsigned section. The unsigned section contains a set of key/signature pairs,
// and the signed section is the encoding of another protocol buffer. All
// signatures cover [prefix][signed-header-size][signed-header][archive].
VerifierResult VerifyCrx3(
    base::File* file,
    crypto::SecureHash* hash,
    const std::vector<std::vector<uint8_t>>& required_key_hashes,
    std::string* public_key,
    std::string* crx_id,
    std::vector<uint8_t>* compressed_verified_contents,
    bool require_publisher_key,
    bool accept_publisher_test_key) {
  // Parse [header-size] and [header].
  int header_size =
      base::saturated_cast<int>(ReadAndHashLittleEndianUInt32(file, hash));
  if (header_size == INT_MAX) {
    return VerifierResult::ERROR_HEADER_INVALID;
  }
  std::vector<uint8_t> header_bytes(header_size);
  if (ReadAndHashBuffer(header_bytes.data(), header_size, file, hash) !=
      header_size) {
    return VerifierResult::ERROR_HEADER_INVALID;
  }

  // If the header contains a ZIP EOCD or EOCD64 token, unzipping may not work
  // correctly.
  if (std::search(std::begin(header_bytes), std::end(header_bytes),
                  std::begin(kEocd),
                  std::end(kEocd)) != std::end(header_bytes) ||
      std::search(std::begin(header_bytes), std::end(header_bytes),
                  std::begin(kEocd64),
                  std::end(kEocd64)) != std::end(header_bytes)) {
    return VerifierResult::ERROR_HEADER_INVALID;
  }

  CrxFileHeader header;
  if (!header.ParseFromArray(header_bytes.data(), header_size)) {
    return VerifierResult::ERROR_HEADER_INVALID;
  }

  // Parse [verified_contents].
  if (header.has_verified_contents() && compressed_verified_contents) {
    const std::string& header_verified_contents(header.verified_contents());
    compressed_verified_contents->assign(header_verified_contents.begin(),
                                         header_verified_contents.end());
  }

  // Parse [signed-header].
  const std::string& signed_header_data_str = header.signed_header_data();
  SignedData signed_header_data;
  if (!signed_header_data.ParseFromString(signed_header_data_str)) {
    return VerifierResult::ERROR_HEADER_INVALID;
  }
  const std::string& crx_id_encoded = signed_header_data.crx_id();
  const std::string declared_crx_id =
      id_util::GenerateIdFromHex(base::HexEncode(crx_id_encoded));

  // Create a little-endian representation of [signed-header-size].
  const int signed_header_size = signed_header_data_str.size();
  const uint8_t header_size_octets[] = {
      static_cast<uint8_t>(signed_header_size),
      static_cast<uint8_t>(signed_header_size >> 8),
      static_cast<uint8_t>(signed_header_size >> 16),
      static_cast<uint8_t>(signed_header_size >> 24)};

  // Create a set of all required key hashes.
  std::set<std::vector<uint8_t>> required_key_set(required_key_hashes.begin(),
                                                  required_key_hashes.end());

  using ProofFetcher = const RepeatedProof& (CrxFileHeader::*)() const;
  ProofFetcher rsa = &CrxFileHeader::sha256_with_rsa;
  ProofFetcher ecdsa = &CrxFileHeader::sha256_with_ecdsa;

  std::string public_key_bytes;
  VerifierCollection verifiers;
  verifiers.reserve(header.sha256_with_rsa_size() +
                    header.sha256_with_ecdsa_size());
  const std::vector<
      std::pair<ProofFetcher, crypto::SignatureVerifier::SignatureAlgorithm>>
      proof_types = {
          std::make_pair(rsa, crypto::SignatureVerifier::RSA_PKCS1_SHA256),
          std::make_pair(ecdsa, crypto::SignatureVerifier::ECDSA_SHA256)};

  std::vector<uint8_t> publisher_key(std::begin(kPublisherKeyHash),
                                     std::end(kPublisherKeyHash));
  std::optional<std::vector<uint8_t>> publisher_test_key;
  if (accept_publisher_test_key) {
    publisher_test_key.emplace(std::begin(kPublisherTestKeyHash),
                               std::end(kPublisherTestKeyHash));
  }
  bool found_publisher_key = false;

  // Initialize all verifiers and update them with
  // [prefix][signed-header-size][signed-header].
  // Clear any elements of required_key_set that are encountered, and watch for
  // the developer key.
  for (const auto& proof_type : proof_types) {
    for (const auto& proof : (header.*proof_type.first)()) {
      const std::string& key = proof.public_key();
      const std::string& sig = proof.signature();
      if (id_util::GenerateId(key) == declared_crx_id) {
        public_key_bytes = key;
      }
      std::vector<uint8_t> key_hash(crypto::kSHA256Length);
      crypto::SHA256HashString(key, key_hash.data(), key_hash.size());
      required_key_set.erase(key_hash);
      DCHECK_EQ(accept_publisher_test_key, publisher_test_key.has_value());
      found_publisher_key =
          found_publisher_key || key_hash == publisher_key ||
          (accept_publisher_test_key && key_hash == *publisher_test_key);
      auto v = std::make_unique<crypto::SignatureVerifier>();
      static_assert(sizeof(unsigned char) == sizeof(uint8_t),
                    "Unsupported char size.");
      if (!v->VerifyInit(proof_type.second,
                         base::as_bytes(base::make_span(sig)),
                         base::as_bytes(base::make_span(key)))) {
        return VerifierResult::ERROR_SIGNATURE_INITIALIZATION_FAILED;
      }
      v->VerifyUpdate(base::as_bytes(base::make_span(kSignatureContext)));
      v->VerifyUpdate(header_size_octets);
      v->VerifyUpdate(base::as_bytes(base::make_span(signed_header_data_str)));
      verifiers.push_back(std::move(v));
    }
  }
  if (public_key_bytes.empty() || !required_key_set.empty()) {
    return VerifierResult::ERROR_REQUIRED_PROOF_MISSING;
  }

  if (require_publisher_key && !found_publisher_key) {
    return VerifierResult::ERROR_REQUIRED_PROOF_MISSING;
  }

  // Update and finalize the verifiers with [archive].
  if (!ReadHashAndVerifyArchive(file, hash, verifiers)) {
    return VerifierResult::ERROR_SIGNATURE_VERIFICATION_FAILED;
  }

  *public_key = base::Base64Encode(public_key_bytes);
  *crx_id = declared_crx_id;
  return VerifierResult::OK_FULL;
}

}  // namespace

VerifierResult Verify(
    const base::FilePath& crx_path,
    const VerifierFormat& format,
    const std::vector<std::vector<uint8_t>>& required_key_hashes,
    const std::vector<uint8_t>& required_file_hash,
    std::string* public_key,
    std::string* crx_id,
    std::vector<uint8_t>* compressed_verified_contents) {
  std::string public_key_local;
  std::string crx_id_local;
  base::File file(crx_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return VerifierResult::ERROR_FILE_NOT_READABLE;
  }

  std::unique_ptr<crypto::SecureHash> file_hash =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);

  // Magic number.
  bool diff = false;
  char buffer[kCrxFileHeaderMagicSize] = {};
  if (file.ReadAtCurrentPos(buffer, kCrxFileHeaderMagicSize) !=
      kCrxFileHeaderMagicSize) {
    return VerifierResult::ERROR_HEADER_INVALID;
  }
  if (!strncmp(buffer, kCrxDiffFileHeaderMagic, kCrxFileHeaderMagicSize)) {
    diff = true;
  } else if (strncmp(buffer, kCrxFileHeaderMagic, kCrxFileHeaderMagicSize)) {
    return VerifierResult::ERROR_HEADER_INVALID;
  }
  file_hash->Update(buffer, sizeof(buffer));

  // Version number.
  const uint32_t version =
      ReadAndHashLittleEndianUInt32(&file, file_hash.get());
  VerifierResult result;
  if (version == 3) {
    bool require_publisher_key =
        format == VerifierFormat::CRX3_WITH_PUBLISHER_PROOF ||
        format == VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF;
    result = VerifyCrx3(
        &file, file_hash.get(), required_key_hashes, &public_key_local,
        &crx_id_local, compressed_verified_contents, require_publisher_key,
        format == VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF);
  } else {
    result = VerifierResult::ERROR_HEADER_INVALID;
  }
  if (result != VerifierResult::OK_FULL) {
    return result;
  }

  // Finalize file hash.
  uint8_t final_hash[crypto::kSHA256Length] = {};
  file_hash->Finish(final_hash, sizeof(final_hash));
  if (!required_file_hash.empty()) {
    if (required_file_hash.size() != crypto::kSHA256Length) {
      return VerifierResult::ERROR_EXPECTED_HASH_INVALID;
    }
    if (!crypto::SecureMemEqual(final_hash, required_file_hash.data(),
                                crypto::kSHA256Length)) {
      return VerifierResult::ERROR_FILE_HASH_FAILED;
    }
  }

  // All is well. Set the out-params and return.
  if (public_key) {
    *public_key = public_key_local;
  }
  if (crx_id) {
    *crx_id = crx_id_local;
  }
  return diff ? VerifierResult::OK_DELTA : VerifierResult::OK_FULL;
}

}  // namespace crx_file
