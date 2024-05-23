// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/encryption/test_encryption_module.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/statusor.h"

using ::testing::Invoke;

namespace reporting {
namespace test {

TestEncryptionModuleStrict::TestEncryptionModuleStrict() {
  ON_CALL(*this, EncryptRecordImpl)
      .WillByDefault(
          Invoke([](std::string_view record,
                    base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) {
            EncryptedRecord encrypted_record;
            encrypted_record.set_encrypted_wrapped_record(std::string(record));
            // encryption_info is not set.
            std::move(cb).Run(encrypted_record);
          }));
}

void TestEncryptionModuleStrict::UpdateAsymmetricKeyImpl(
    std::string_view new_public_key,
    PublicKeyId new_public_key_id,
    base::OnceCallback<void(Status)> response_cb) {
  // Ignore keys but return success.
  std::move(response_cb).Run(Status(Status::StatusOK()));
}

TestEncryptionModuleStrict::~TestEncryptionModuleStrict() = default;

}  // namespace test
}  // namespace reporting
