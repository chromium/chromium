// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/task_environment.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/services/libassistant/grpc/assistant_client_v1.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace libassistant {

namespace {

class AssistantManagerInternalMock
    : public assistant::FakeAssistantManagerInternal {
 public:
  AssistantManagerInternalMock() = default;
  AssistantManagerInternalMock(const AssistantManagerInternalMock&) = delete;
  AssistantManagerInternalMock& operator=(const AssistantManagerInternalMock&) =
      delete;
  ~AssistantManagerInternalMock() override = default;

  // assistant::FakeAssistantManagerInternal implementation:
  MOCK_METHOD(void, SetLocaleOverride, (const std::string& locale));
  MOCK_METHOD(void,
              SetOptions,
              (const assistant_client::InternalOptions& options,
               assistant_client::SuccessCallbackInternal on_done));
};

}  // namespace

class AssistantClientV1Test : public testing::Test {
 public:
  AssistantClientV1Test() = default;
  AssistantClientV1Test(const AssistantClientV1Test&) = delete;
  AssistantClientV1Test& operator=(const AssistantClientV1Test&) = delete;
  ~AssistantClientV1Test() override = default;

  void SetUp() override {
    auto assistant_manager =
        std::make_unique<assistant::FakeAssistantManager>();
    assistant_manager_internal_ =
        std::make_unique<testing::StrictMock<AssistantManagerInternalMock>>();

    assistant_client_ = std::make_unique<AssistantClientV1>(
        std::move(assistant_manager), assistant_manager_internal_.get());

    assistant_client_->StartServices();
  }

  AssistantClientV1& v1_client() { return *assistant_client_; }

  AssistantManagerInternalMock& assistant_manager_internal_mock() {
    return *assistant_manager_internal_;
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  std::unique_ptr<AssistantClientV1> assistant_client_ = nullptr;
  std::unique_ptr<AssistantManagerInternalMock> assistant_manager_internal_ =
      nullptr;
};

TEST_F(AssistantClientV1Test, ShouldSetLocale) {
  EXPECT_CALL(assistant_manager_internal_mock(), SetLocaleOverride("locale"));

  v1_client().SetLocaleOverride("locale");
}

TEST_F(AssistantClientV1Test, ShouldSetOptions) {
  v1_client().SetDeviceAttributes(/*dark_mode_enabled=*/true);

  EXPECT_CALL(assistant_manager_internal_mock(), SetOptions);

  v1_client().SetInternalOptions("locale", true);
}

}  // namespace libassistant
}  // namespace chromeos
