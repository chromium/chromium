// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_ENCRYPTION_TEST_ENCRYPTION_MODULE_H_
#define COMPONENTS_REPORTING_ENCRYPTION_TEST_ENCRYPTION_MODULE_H_

#include <string_view>

#include "base/functional/callback.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/statusor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace test {

// An |EncryptionModuleInterface| that does no encryption.
class TestEncryptionModuleStrict : public EncryptionModuleInterface {
 public:
  TestEncryptionModuleStrict();

  MOCK_METHOD(void,
              EncryptRecordImpl,
              (std::string_view record,
               base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb),
              (const override));

  void UpdateAsymmetricKeyImpl(
      std::string_view new_public_key,
      PublicKeyId new_public_key_id,
      base::OnceCallback<void(Status)> response_cb) override;

 protected:
  ~TestEncryptionModuleStrict() override;
};

// Most of the time no need to log uninterested calls to |EncryptRecord|.
typedef ::testing::NiceMock<TestEncryptionModuleStrict> TestEncryptionModule;

}  // namespace test
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_ENCRYPTION_TEST_ENCRYPTION_MODULE_H_
