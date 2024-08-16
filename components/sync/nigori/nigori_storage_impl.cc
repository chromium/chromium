// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_storage_impl.h"

#include <string>

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/sync/protocol/nigori_local_data.pb.h"

namespace syncer {

NigoriStorageImpl::NigoriStorageImpl(const base::FilePath& path)
    : path_(path) {}

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
  if (!OSCrypt::EncryptString(serialized_data, &encrypted_data)) {
    DLOG(ERROR) << "Failed to encrypt NigoriLocalData.";
    return;
  }

  if (!base::ImportantFileWriter::WriteFileAtomically(path_, encrypted_data,
                                                      "Nigori")) {
    DLOG(ERROR) << "Failed to write NigoriLocalData into file.";
  }
}

std::optional<sync_pb::NigoriLocalData> NigoriStorageImpl::RestoreData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::PathExists(path_)) {
    return std::nullopt;
  }

  std::string encrypted_data;
  if (!base::ReadFileToString(path_, &encrypted_data)) {
    DLOG(ERROR) << "Failed to read NigoriLocalData from file.";
    return std::nullopt;
  }

  std::string serialized_data;
  if (!OSCrypt::DecryptString(encrypted_data, &serialized_data)) {
    DLOG(ERROR) << "Failed to decrypt NigoriLocalData.";
    return std::nullopt;
  }

  sync_pb::NigoriLocalData data;
  if (!data.ParseFromString(serialized_data)) {
    DLOG(ERROR) << "Failed to parse NigoriLocalData.";
    return std::nullopt;
  }
  return data;
}

void NigoriStorageImpl::ClearData() {
  base::DeleteFile(path_);
}

}  // namespace syncer
