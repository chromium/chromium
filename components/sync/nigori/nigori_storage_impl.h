// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_STORAGE_IMPL_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_STORAGE_IMPL_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "components/sync/nigori/nigori_storage.h"

namespace os_crypt_async {
class Encryptor;
}  // namespace os_crypt_async

namespace syncer {

class NigoriStorageImpl : public NigoriStorage {
 public:
  // If `encryptor` is not null, it will be used for encryption. Otherwise,
  // the synchronous OSCrypt will be used.
  NigoriStorageImpl(const base::FilePath& path,
                    std::unique_ptr<::os_crypt_async::Encryptor> encryptor);

  NigoriStorageImpl(const NigoriStorageImpl&) = delete;
  NigoriStorageImpl& operator=(const NigoriStorageImpl&) = delete;

  ~NigoriStorageImpl() override;

  // NigoriStorage implementation.
  // Encrypts `data` and atomically stores it in binary file.
  void StoreData(const sync_pb::NigoriLocalData& data) override;
  std::optional<sync_pb::NigoriLocalData> RestoreData() override;
  void ClearData() override;

 private:
  base::FilePath path_;
  const std::unique_ptr<::os_crypt_async::Encryptor> encryptor_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_STORAGE_IMPL_H_
