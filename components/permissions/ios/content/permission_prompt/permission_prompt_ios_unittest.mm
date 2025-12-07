// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/ios/content/permission_prompt/permission_prompt_test_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_request.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

class PermissionPromptIOSTest : public content::RenderViewHostTestHarness {
 public:
  PermissionPromptIOSTest() = default;
  ~PermissionPromptIOSTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    SetContents(CreateTestWebContents());
    delegate_ = std::make_unique<MockPermissionPromptDelegate>();
  }

  void TearDown() override {
    delegate_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  MockPermissionPromptDelegate* delegate() { return delegate_.get(); }

  std::unique_ptr<MockPermissionPromptIOS> CreatePrompt() {
    return std::make_unique<MockPermissionPromptIOS>(web_contents(),
                                                     delegate());
  }

  std::unique_ptr<MockPermissionRequest> CreateMockRequest(
      RequestType request_type = RequestType::kGeolocation) {
    return std::make_unique<MockPermissionRequest>(request_type);
  }

 private:
  std::unique_ptr<MockPermissionPromptDelegate> delegate_;
};

TEST_F(PermissionPromptIOSTest, Construction) {
  auto prompt = CreatePrompt();
  EXPECT_NE(nullptr, prompt.get());
}

TEST_F(PermissionPromptIOSTest, ConstructionWithNullWebContents) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        auto prompt =
            std::make_unique<MockPermissionPromptIOS>(nullptr, delegate());
      },
      ".*web_contents.*");
}

TEST_F(PermissionPromptIOSTest, ConstructionWithNullDelegate) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        auto prompt =
            std::make_unique<MockPermissionPromptIOS>(web_contents(), nullptr);
      },
      ".*delegate.*");
}

// Test permission request handling
TEST_F(PermissionPromptIOSTest, PermissionCountWithSingleRequest) {
  delegate()->AddRequest(CreateMockRequest(RequestType::kGeolocation));
  auto prompt = CreatePrompt();

  EXPECT_EQ(1u, prompt->PermissionCount());
}
TEST_F(PermissionPromptIOSTest, PermissionCountWithMultipleRequests) {
  delegate()->AddRequest(CreateMockRequest(RequestType::kMicStream));
  delegate()->AddRequest(CreateMockRequest(RequestType::kCameraStream));
  auto prompt = CreatePrompt();

  EXPECT_EQ(2u, prompt->PermissionCount());
}

TEST_F(PermissionPromptIOSTest, GetContentSettingType) {
  delegate()->AddRequest(CreateMockRequest(RequestType::kGeolocation));
  auto prompt = CreatePrompt();

  EXPECT_EQ(ContentSettingsType::GEOLOCATION, prompt->GetContentSettingType(0));
}

TEST_F(PermissionPromptIOSTest, GetContentSettingTypeOutOfBounds) {
  delegate()->AddRequest(CreateMockRequest(RequestType::kGeolocation));
  auto prompt = CreatePrompt();

  // This should trigger CHECK failure
  EXPECT_DEATH_IF_SUPPORTED(prompt->GetContentSettingType(1), ".*");
}

TEST_F(PermissionPromptIOSTest, GetRequestingOrigin) {
  auto prompt = CreatePrompt();
  EXPECT_EQ(GURL("https://example.com"), prompt->GetRequestingOrigin());
}

TEST_F(PermissionPromptIOSTest, ValidMediaRequestGroup) {
  delegate()->AddRequest(CreateMockRequest(RequestType::kMicStream));
  delegate()->AddRequest(CreateMockRequest(RequestType::kCameraStream));
  auto prompt = CreatePrompt();

  // Should not crash when getting message text for valid grouped requests
  auto message_text = prompt->GetAnnotatedMessageText();
  EXPECT_FALSE(message_text.text.empty());
}

TEST_F(PermissionPromptIOSTest, SingleRequestMessageText) {
  delegate()->AddRequest(CreateMockRequest(RequestType::kGeolocation));
  auto prompt = CreatePrompt();

  auto message_text = prompt->GetAnnotatedMessageText();
  EXPECT_FALSE(message_text.text.empty());
}

TEST_F(PermissionPromptIOSTest, RequestsAreCopiedToWeakPointers) {
  auto mock_request = CreateMockRequest(RequestType::kGeolocation);
  auto* request_ptr = mock_request.get();
  delegate()->AddRequest(std::move(mock_request));

  auto prompt = CreatePrompt();
  const auto& requests = prompt->Requests();

  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(request_ptr, requests[0].get());
}

}  // namespace permissions
