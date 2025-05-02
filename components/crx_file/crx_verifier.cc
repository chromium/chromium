// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "components/crx_file/crx3.pb.h"
#include "components/crx_file/crx_file.h"
#include "components/crx_file/id_util.h"
#include "crypto/hash.h"
#include "crypto/secure_util.h"
#include "crypto/signature_verifier.h"

namespace crx_file {

namespace {

using KeyHash = std::array<uint8_t, crypto::hash::kSha256Size>;

// The SHA256 hash of the DER SPKI "ecdsa_2017_public" Crx3 key.
constexpr KeyHash kPublisherKeyHash = {
    0x61, 0xf7, 0xf2, 0xa6, 0xbf, 0xcf, 0x74, 0xcd, 0x0b, 0xc1, 0xfe,
    0x24, 0x97, 0xcc, 0x9b, 0x04, 0x25, 0x4c, 0x65, 0x8f, 0x79, 0xf2,
    0x14, 0x53, 0x92, 0x86, 0x7e, 0xa8, 0x36, 0x63, 0x67, 0xcf};

// The SHA256 hash of the DER SPKI "ecdsa_2017_public" Crx3 test key.
constexpr KeyHash kPublisherTestKeyHash = {
    0x6c, 0x46, 0x41, 0x3b, 0x00, 0xd0, 0xfa, 0x0e, 0x72, 0xc8, 0xd2,
    0x5f, 0x64, 0xf3, 0xa6, 0x17, 0x03, 0x0d, 0xde, 0x21, 0x61, 0xbe,
    0xb7, 0x95, 0x91, 0x95, 0x83, 0x68, 0x12, 0xe9, 0x78, 0x1e};

constexpr auto kEocd = std::to_array<uint8_t>({'P', 'K', 0x05, 0x06});
constexpr auto kEocd64 = std::to_array<uint8_t>({'P', 'K', 0x06, 0x07});

using VerifierCollection =
    std::vector<std::unique_ptr<crypto::SignatureVerifier>>;
using RepeatedProof = google::protobuf::RepeatedPtrField<AsymmetricKeyProof>;

std::optional<size_t> ReadAndHashBuffer(base::span<uint8_t> buffer,
                                        base::File* file,
                                        crypto::hash::Hasher& hash) {
  auto read = file->ReadAtCurrentPos(buffer);
  if (read.value_or(0) > 0) {
    hash.Update(buffer.first(*read));
  }
  return read;
}

// Returns UINT32_MAX in the case of an unexpected EOF or read error, else
// returns the read uint32.
uint32_t ReadAndHashLittleEndianUInt32(base::File* file,
                                       crypto::hash::Hasher& hash) {
  std::array<uint8_t, 4> buffer;
  if (ReadAndHashBuffer(buffer, file, hash).value_or(4) != buffer.size()) {
    return UINT32_MAX;
  }
  return base::I32FromLittleEndian(buffer);
}

// Read to the end of the file, updating the hash and all verifiers.
bool ReadHashAndVerifyArchive(base::File* file,
                              crypto::hash::Hasher& hash,
                              const VerifierCollection& verifiers) {
  std::array<uint8_t, 1 << 12> buffer;
  std::optional<size_t> len;
  while ((len = ReadAndHashBuffer(buffer, file, hash)).value_or(0) > 0) {
    auto to_verify = base::span<const uint8_t>(buffer).first(*len);
    for (auto& verifier : verifiers) {
      verifier->VerifyUpdate(to_verify);
    }
  }
  for (auto& verifier : verifiers) {
    if (!verifier->VerifyFinal()) {
      return false;
    }
  }
  // A final read with a length of 0 signals the end of the input file. A read
  // with no length at all signals a read error and should be treated as a
  // failure.
  return len.has_value() && len.value() == 0;
}

// The remaining contents of a Crx3 file are [header-size][header][archive].
// [header] is an encoded protocol buffer and contains both a signed and
// unsigned section. The unsigned section contains a set of key/signature pairs,
// and the signed section is the encoding of another protocol buffer. All
// signatures cover [prefix][signed-header-size][signed-header][archive].
VerifierResult VerifyCrx3(
    base::File* file,
    crypto::hash::Hasher& hash,
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
  if (ReadAndHashBuffer(header_bytes, file, hash) != header_size) {
    return VerifierResult::ERROR_HEADER_INVALID;
  }

  // If the header contains a ZIP EOCD or EOCD64 token, unzipping may not work
  // correctly.
  if (std::ranges::search(header_bytes, kEocd) ||
      std::ranges::search(header_bytes, kEocd64)) {
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
  const auto header_size_octets =
      base::I32ToLittleEndian(signed_header_data_str.size());

  // Create a set of all required key hashes.
  std::set<KeyHash> required_key_set;
  for (const auto& key_hash : required_key_hashes) {
    KeyHash hash_copy;
    base::span<uint8_t>(hash_copy).copy_from(key_hash);
    required_key_set.insert(hash_copy);
  }

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
      auto key_hash = crypto::hash::Sha256(key);
      required_key_set.erase(key_hash);
      found_publisher_key =
          found_publisher_key || key_hash == kPublisherKeyHash ||
          (accept_publisher_test_key && key_hash == kPublisherTestKeyHash);
      auto v = std::make_unique<crypto::SignatureVerifier>();
      if (!v->VerifyInit(proof_type.second, base::as_byte_span(sig),
                         base::as_byte_span(key))) {
        return VerifierResult::ERROR_SIGNATURE_INITIALIZATION_FAILED;
      }
      v->VerifyUpdate(base::as_byte_span(kSignatureContext));
      v->VerifyUpdate(header_size_octets);
      v->VerifyUpdate(base::as_byte_span(signed_header_data_str));
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

  crypto::hash::Hasher file_hash(crypto::hash::HashKind::kSha256);

  // Magic number.
  bool diff = false;
  std::array<uint8_t, std::size(kCrxFileHeaderMagic)> buffer;
  if (!file.ReadAtCurrentPosAndCheck(buffer)) {
    return VerifierResult::ERROR_HEADER_INVALID;
  }
  if (std::ranges::equal(buffer, kCrxDiffFileHeaderMagic)) {
    diff = true;
  } else if (!std::ranges::equal(buffer, kCrxFileHeaderMagic)) {
    return VerifierResult::ERROR_HEADER_INVALID;
  }
  file_hash.Update(buffer);

  // Version number.
  const uint32_t version = ReadAndHashLittleEndianUInt32(&file, file_hash);
  VerifierResult result;
  if (version == 3) {
    bool require_publisher_key =
        format == VerifierFormat::CRX3_WITH_PUBLISHER_PROOF ||
        format == VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF;
    result = VerifyCrx3(
        &file, file_hash, required_key_hashes, &public_key_local, &crx_id_local,
        compressed_verified_contents, require_publisher_key,
        format == VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF);
  } else {
    result = VerifierResult::ERROR_HEADER_INVALID;
  }
  if (result != VerifierResult::OK_FULL) {
    return result;
  }

  // Finalize file hash.
  std::array<uint8_t, crypto::hash::kSha256Size> final_hash;
  file_hash.Finish(final_hash);
  if (!required_file_hash.empty()) {
    if (required_file_hash.size() != crypto::hash::kSha256Size) {
      return VerifierResult::ERROR_EXPECTED_HASH_INVALID;
    }
    if (!crypto::SecureMemEqual(final_hash, required_file_hash)) {
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
