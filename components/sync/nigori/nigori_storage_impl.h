// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_STORAGE_IMPL_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_STORAGE_IMPL_H_

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "components/sync/nigori/nigori_storage.h"

namespace syncer {

class NigoriStorageImpl : public NigoriStorage {
 public:
  // |encryptor| must be not null and must outlive this object.
  explicit NigoriStorageImpl(const base::FilePath& path);

  NigoriStorageImpl(const NigoriStorageImpl&) = delete;
  NigoriStorageImpl& operator=(const NigoriStorageImpl&) = delete;

  ~NigoriStorageImpl() override;

  // NigoriStorage implementation.
  // Encrypts |data| and atomically stores it in binary file.
  void StoreData(const sync_pb::NigoriLocalData& data) override;
  std::optional<sync_pb::NigoriLocalData> RestoreData() override;
  void ClearData() override;

 private:
  base::FilePath path_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_STORAGE_IMPL_H_
