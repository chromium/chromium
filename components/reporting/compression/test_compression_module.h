// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_COMPRESSION_TEST_COMPRESSION_MODULE_H_
#define COMPONENTS_REPORTING_COMPRESSION_TEST_COMPRESSION_MODULE_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
       scoped_refptr<ResourceManager> memory_resource,
       base::OnceCallback<void(std::string,
                               std::optional<CompressionInformation>)> cb),
      (const override));

 protected:
  ~TestCompressionModuleStrict() override;
};

// Most of the time no need to log uninterested calls to |EncryptRecord|.
typedef ::testing::NiceMock<TestCompressionModuleStrict> TestCompressionModule;

}  // namespace test
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_COMPRESSION_TEST_COMPRESSION_MODULE_H_
