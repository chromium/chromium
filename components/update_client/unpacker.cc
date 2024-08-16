// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/unpacker.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "third_party/zlib/google/compression_utils.h"

namespace {

constexpr base::FilePath::CharType kMetadataFolder[] =
    FILE_PATH_LITERAL("_metadata");

base::FilePath GetVerifiedContentsPath(const base::FilePath& extension_path) {
  return extension_path.Append(kMetadataFolder)
      .Append(FILE_PATH_LITERAL("verified_contents.json"));
}

}  // namespace

namespace update_client {

Unpacker::Result::Result() = default;

Unpacker::Unpacker(const base::FilePath& path,
                   std::unique_ptr<Unzipper> unzipper,
                   base::OnceCallback<void(const Result& result)> callback)
    : path_(path),
      unzipper_(std::move(unzipper)),
      callback_(std::move(callback)) {}

Unpacker::~Unpacker() = default;

void Unpacker::Unpack(const std::vector<uint8_t>& pk_hash,
                      const base::FilePath& path,
                      std::unique_ptr<Unzipper> unzipper,
                      crx_file::VerifierFormat crx_format,
                      base::OnceCallback<void(const Result& result)> callback) {
  base::WrapRefCounted(
      new Unpacker(path, std::move(unzipper), std::move(callback)))
      ->Verify(pk_hash, crx_format);
}

void Unpacker::Verify(const std::vector<uint8_t>& pk_hash,
                      crx_file::VerifierFormat crx_format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Verifying component: " << path_.value();
  if (path_.empty()) {
    EndUnpacking(UnpackerError::kInvalidParams, 0);
    return;
  }
  std::vector<std::vector<uint8_t>> required_keys;
  if (!pk_hash.empty()) {
    required_keys.push_back(pk_hash);
  }
  const crx_file::VerifierResult result = crx_file::Verify(
      path_, crx_format, required_keys, std::vector<uint8_t>(), &public_key_,
      /*crx_id=*/nullptr, &compressed_verified_contents_);
  if (result != crx_file::VerifierResult::OK_FULL) {
    EndUnpacking(UnpackerError::kInvalidFile, static_cast<int>(result));
    return;
  }
  VLOG(2) << "Verification successful: " << path_.value();
  BeginUnzipping();
}

void Unpacker::BeginUnzipping() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::CreateNewTempDirectory(
          FILE_PATH_LITERAL("chrome_Unpacker_BeginUnzipping"), &unpack_path_)) {
    VLOG(1) << "Unable to create temporary directory for unpacking.";
    EndUnpacking(UnpackerError::kUnzipPathError,
                 ::logging::GetLastSystemErrorCode());
    return;
  }
  VLOG(1) << "Unpacking in: " << unpack_path_.value();
  unzipper_->Unzip(path_, unpack_path_,
                   base::BindOnce(&Unpacker::EndUnzipping, this));
}

void Unpacker::EndUnzipping(bool result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result) {
    VLOG(1) << "Unzipping failed.";
    EndUnpacking(UnpackerError::kUnzipFailed, 0);
    return;
  }
  VLOG(2) << "Unzipped successfully";
  // Ignore the verified contents in the header if the verified
  // contents are already present in the _metadata folder.
  if (compressed_verified_contents_.empty() ||
      base::PathExists(GetVerifiedContentsPath(unpack_path_))) {
    EndUnpacking(UnpackerError::kNone);
    return;
  }

  UncompressVerifiedContents();
}

void Unpacker::UncompressVerifiedContents() {
  std::string verified_contents;
  if (!compression::GzipUncompress(compressed_verified_contents_,
                                   &verified_contents)) {
    VLOG(1) << "Decompressing verified contents from header failed";
    EndUnpacking(UnpackerError::kNone);
    return;
  }

  StoreVerifiedContentsInExtensionDir(verified_contents);
}

void Unpacker::StoreVerifiedContentsInExtensionDir(
    const std::string& verified_contents) {
  base::FilePath metadata_path = unpack_path_.Append(kMetadataFolder);
  if (!base::CreateDirectory(metadata_path)) {
    VLOG(1) << "Could not create metadata directory " << metadata_path;
    EndUnpacking(UnpackerError::kNone);
    return;
  }

  base::FilePath verified_contents_path = GetVerifiedContentsPath(unpack_path_);

  // Cannot write the verified contents file.
  if (!base::WriteFile(verified_contents_path, verified_contents)) {
    VLOG(1) << "Could not write verified contents into file "
            << verified_contents_path;
    EndUnpacking(UnpackerError::kNone);
    return;
  }

  EndUnpacking(UnpackerError::kNone);
}

void Unpacker::EndUnpacking(UnpackerError error, int extended_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error != UnpackerError::kNone && !unpack_path_.empty()) {
    RetryDeletePathRecursively(unpack_path_);
  }

  Result result;
  result.error = error;
  result.extended_error = extended_error;
  if (error == UnpackerError::kNone) {
    VLOG(2) << "Unpacked successfully";
    result.unpack_path = unpack_path_;
    result.public_key = public_key_;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), result));
}

}  // namespace update_client
