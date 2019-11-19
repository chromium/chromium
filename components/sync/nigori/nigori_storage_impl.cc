// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_storage_impl.h"

#include <string>

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "components/sync/base/encryptor.h"

namespace syncer {

NigoriStorageImpl::NigoriStorageImpl(const base::FilePath& path,
                                     const Encryptor* encryptor)
    : path_(path), encryptor_(encryptor) {
  DCHECK(encryptor_);
}

NigoriStorageImpl::~NigoriStorageImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriStorageImpl::StoreData(const sync_pb::NigoriLocalData& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string serialized_data = data.SerializeAsString();
  if (serialized_data.empty()) {
    DLOG(ERROR) << "Failed to serialize NigoriLocalData.";
    return;
  }

  std::string encrypted_data;
  if (!encryptor_->EncryptString(serialized_data, &encrypted_data)) {
    DLOG(ERROR) << "Failed to encrypt NigoriLocalData.";
    return;
  }

  if (!base::ImportantFileWriter::WriteFileAtomically(path_, encrypted_data)) {
    DLOG(ERROR) << "Failed to write NigoriLocalData into file.";
  }
}

base::Optional<sync_pb::NigoriLocalData> NigoriStorageImpl::RestoreData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::PathExists(path_)) {
    return base::nullopt;
  }

  std::string encrypted_data;
  if (!base::ReadFileToString(path_, &encrypted_data)) {
    DLOG(ERROR) << "Failed to read NigoriLocalData from file.";
    return base::nullopt;
  }

  std::string serialized_data;
  if (!encryptor_->DecryptString(encrypted_data, &serialized_data)) {
    DLOG(ERROR) << "Failed to decrypt NigoriLocalData.";
    return base::nullopt;
  }

  sync_pb::NigoriLocalData data;
  if (!data.ParseFromString(serialized_data)) {
    DLOG(ERROR) << "Failed to parse NigoriLocalData.";
    return base::nullopt;
  }
  return data;
}

void NigoriStorageImpl::ClearData() {
  base::DeleteFile(path_, /*recursive=*/false);
}

}  // namespace syncer
