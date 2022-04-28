// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_COMPRESSION_TEST_COMPRESSION_MODULE_H_
#define COMPONENTS_REPORTING_COMPRESSION_TEST_COMPRESSION_MODULE_H_

#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {
namespace test {

// An |CompressionModuleInterface| that does no compression.
class TestCompressionModuleStrict : public CompressionModule {
 public:
  TestCompressionModuleStrict();

  MOCK_METHOD(
      void,
      CompressRecord,
      (std::string record,
       base::OnceCallback<void(std::string,
                               absl::optional<CompressionInformation>)> cb),
      (const override));

 protected:
  ~TestCompressionModuleStrict() override;
};

// Most of the time no need to log uninterested calls to |EncryptRecord|.
typedef ::testing::NiceMock<TestCompressionModuleStrict> TestCompressionModule;

}  // namespace test
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_COMPRESSION_TEST_COMPRESSION_MODULE_H_
