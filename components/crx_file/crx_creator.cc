// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crx_file/crx_creator.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_view_util.h"
#include "components/crx_file/crx3.pb.h"
#include "components/crx_file/crx_file.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"

namespace crx_file {

namespace {

std::string GetCrxId(base::span<const uint8_t> key) {
  const auto full_hash = crypto::hash::Sha256(key);
  const auto truncated_hash = base::span(full_hash).first<16>();
  return std::string(base::as_string_view(truncated_hash));
}

constexpr size_t kFileBufferSize = 1 << 12;

// Read to the end of the file, updating the signer.
CreatorResult ReadAndSignArchive(base::File* file,
                                 crypto::sign::Signer& signer,
                                 std::vector<uint8_t>* signature) {
  std::array<uint8_t, kFileBufferSize> buffer;
  std::optional<size_t> read;
  while ((read = file->ReadAtCurrentPos(buffer)).value_or(0) > 0) {
    signer.Update(base::span(buffer).first(*read));
  }
  if (!read.has_value()) {
    return CreatorResult::ERROR_SIGNING_FAILURE;
  }
  *signature = signer.Finish();
  return CreatorResult::OK;
}

bool WriteArchive(base::File* out, base::File* in) {
  std::array<uint8_t, kFileBufferSize> buffer;
  std::optional<size_t> read;
  in->Seek(base::File::Whence::FROM_BEGIN, 0);
  while ((read = in->ReadAtCurrentPos(buffer)).value_or(0) > 0) {
    auto to_write = base::span<const uint8_t>(buffer).first(*read);
    if (!out->WriteAtCurrentPosAndCheck(to_write)) {
      return false;
    }
  }
  // A successful final read at the end of the file is indicated by returning a
  // populated option with a read size of 0. An unpopulated option indicates a
  // read error.
  return read.has_value() && read.value() == 0;
}

CreatorResult SignArchiveAndCreateHeader(
    const base::FilePath& output_path,
    base::File* file,
    const crypto::keypair::PrivateKey& signing_key,
    CrxFileHeader* header) {
  // Get the public key.
  std::vector<uint8_t> public_key = signing_key.ToSubjectPublicKeyInfo();

  // Assemble SignedData section.
  SignedData signed_header_data;
  signed_header_data.set_crx_id(GetCrxId(public_key));
  const std::string signed_header_data_str =
      signed_header_data.SerializeAsString();
  const auto signed_header_size_octets =
      base::I32ToLittleEndian(signed_header_data_str.size());

  // Create a signer, init with purpose, SignedData length, run SignedData
  // through, run ZIP through.
  crypto::sign::Signer signer(crypto::sign::SignatureKind::RSA_PKCS1_SHA256,
                              signing_key);
  signer.Update(base::as_byte_span(kSignatureContext));
  signer.Update(signed_header_size_octets);
  signer.Update(base::as_byte_span(signed_header_data_str));

  if (!file->IsValid()) {
    return CreatorResult::ERROR_FILE_NOT_READABLE;
  }
  std::vector<uint8_t> signature;
  const CreatorResult signing_result =
      ReadAndSignArchive(file, signer, &signature);
  if (signing_result != CreatorResult::OK) {
    return signing_result;
  }
  AsymmetricKeyProof* proof = header->add_sha256_with_rsa();
  proof->set_public_key(base::as_string_view(public_key));
  proof->set_signature(base::as_string_view(signature));
  header->set_signed_header_data(signed_header_data_str);
  return CreatorResult::OK;
}

CreatorResult WriteCRX(const CrxFileHeader& header,
                       const base::FilePath& output_path,
                       base::File* file) {
  const std::string header_str = header.SerializeAsString();
  const auto header_size_octets = base::I32ToLittleEndian(header_str.size());

  const auto format_version_octets = std::to_array<uint8_t>({3, 0, 0, 0});
  base::File crx(output_path,
                 base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!crx.IsValid()) {
    return CreatorResult::ERROR_FILE_NOT_WRITABLE;
  }
  if (!crx.WriteAtCurrentPosAndCheck(kCrxFileHeaderMagic) ||
      !crx.WriteAtCurrentPosAndCheck(format_version_octets) ||
      !crx.WriteAtCurrentPosAndCheck(header_size_octets) ||
      !crx.WriteAtCurrentPosAndCheck(base::as_byte_span(header_str)) ||
      !WriteArchive(&crx, file)) {
    return CreatorResult::ERROR_FILE_WRITE_FAILURE;
  }
  return CreatorResult::OK;
}

}  // namespace

CreatorResult CreateCrxWithVerifiedContentsInHeader(
    const base::FilePath& output_path,
    const base::FilePath& zip_path,
    const crypto::keypair::PrivateKey& signing_key,
    const std::string& verified_contents) {
  CrxFileHeader header;
  base::File file(zip_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                base::File::FLAG_WIN_SHARE_DELETE);
  PLOG_IF(ERROR, !file.IsValid())
      << "Failed to open " << zip_path << ": " << file.error_details();
  const CreatorResult signing_result =
      SignArchiveAndCreateHeader(output_path, &file, signing_key, &header);
  if (signing_result != CreatorResult::OK) {
    return signing_result;
  }

  // Inject the verified contents into the header.
  header.set_verified_contents(verified_contents);
  const CreatorResult result = WriteCRX(header, output_path, &file);
  return result;
}

CreatorResult Create(const base::FilePath& output_path,
                     const base::FilePath& zip_path,
                     const crypto::keypair::PrivateKey& signing_key) {
  CrxFileHeader header;
  base::File file(zip_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                base::File::FLAG_WIN_SHARE_DELETE);
  PLOG_IF(ERROR, !file.IsValid())
      << "Failed to open " << zip_path << ": " << file.error_details();
  const CreatorResult signing_result =
      SignArchiveAndCreateHeader(output_path, &file, signing_key, &header);
  if (signing_result != CreatorResult::OK) {
    return signing_result;
  }

  const CreatorResult result = WriteCRX(header, output_path, &file);
  return result;
}

}  // namespace crx_file
