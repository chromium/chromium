// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/puffin_component_unpacker.h"

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

namespace update_client {

PuffinComponentUnpacker::Result::Result() = default;

PuffinComponentUnpacker::PuffinComponentUnpacker(
    const std::vector<uint8_t>& pk_hash,
    const base::FilePath& path,
    std::unique_ptr<Unzipper> unzipper,
    crx_file::VerifierFormat crx_format,
    base::OnceCallback<void(const Result& result)> callback)
    : pk_hash_(pk_hash),
      path_(path),
      unzipper_(std::move(unzipper)),
      crx_format_(crx_format),
      callback_(std::move(callback)) {}

PuffinComponentUnpacker::~PuffinComponentUnpacker() = default;

void PuffinComponentUnpacker::Unpack(
    const std::vector<uint8_t>& pk_hash,
    const base::FilePath& path,
    std::unique_ptr<Unzipper> unzipper,
    crx_file::VerifierFormat crx_format,
    base::OnceCallback<void(const Result& result)> callback) {
  scoped_refptr<PuffinComponentUnpacker> unpacker =
      base::WrapRefCounted(new PuffinComponentUnpacker(
          pk_hash, path, std::move(unzipper), crx_format, std::move(callback)));
  unpacker->Verify();
}

void PuffinComponentUnpacker::Verify() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Verifying component: " << path_.value();
  if (path_.empty()) {
    EndUnpacking(UnpackerError::kInvalidParams, 0);
    return;
  }
  std::vector<std::vector<uint8_t>> required_keys;
  if (!pk_hash_.empty())
    required_keys.push_back(pk_hash_);
  const crx_file::VerifierResult result = crx_file::Verify(
      path_, crx_format_, required_keys, std::vector<uint8_t>(), &public_key_,
      /*crx_id=*/nullptr, /*compressed_verified_contents=*/nullptr);
  if (result != crx_file::VerifierResult::OK_FULL) {
    EndUnpacking(UnpackerError::kInvalidFile, static_cast<int>(result));
    return;
  }
  VLOG(2) << "Verification successful: " << path_.value();
  BeginUnzipping();
}

void PuffinComponentUnpacker::BeginUnzipping() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath& destination = unpack_path_;
  if (!base::CreateNewTempDirectory(base::FilePath::StringType(),
                                    &destination)) {
    VLOG(1) << "Unable to create temporary directory for unpacking.";
    EndUnpacking(UnpackerError::kUnzipPathError, 0);
    return;
  }
  VLOG(1) << "Unpacking in: " << destination.value();
  unzipper_->Unzip(
      path_, destination,
      base::BindOnce(&PuffinComponentUnpacker::EndUnzipping, this));
}

void PuffinComponentUnpacker::EndUnzipping(bool result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result) {
    VLOG(1) << "Unzipping failed.";
    EndUnpacking(UnpackerError::kUnzipFailed, 0);
    return;
  }
  VLOG(2) << "Unzipped successfully";
  EndUnpacking(UnpackerError::kNone, 0);
}

void PuffinComponentUnpacker::EndUnpacking(UnpackerError error,
                                           int extended_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error != UnpackerError::kNone && !unpack_path_.empty())
    base::DeletePathRecursively(unpack_path_);

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
