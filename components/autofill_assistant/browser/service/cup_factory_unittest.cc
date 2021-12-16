// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/cup_factory.h"
#include "components/autofill_assistant/browser/service/cup_impl.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class CUPFactoryTest : public testing::Test {
 public:
  CUPFactoryTest()
      : cup_factory_{
            std::make_unique<autofill_assistant::cup::CUPImplFactory>()} {}
  ~CUPFactoryTest() override = default;

 protected:
  std::unique_ptr<autofill_assistant::cup::CUPFactory> cup_factory_;
};

TEST_F(CUPFactoryTest, ShouldCreateCupImplInstance) {
  std::unique_ptr<autofill_assistant::cup::CUP> cup =
      cup_factory_->CreateInstance(autofill_assistant::RpcType::GET_ACTIONS);
  ASSERT_NE(cup, nullptr);

  std::string packed_request = cup->PackAndSignRequest("request");
  EXPECT_FALSE(packed_request.empty());
}

}  // namespace
