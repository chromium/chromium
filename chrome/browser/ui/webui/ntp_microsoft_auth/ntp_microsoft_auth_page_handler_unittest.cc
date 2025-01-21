// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_page_handler.h"

#include <memory>

#include "base/test/gmock_move_support.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockMicrosoftAuthService : public MicrosoftAuthService {
 public:
  MOCK_METHOD1(SetAccessToken, void(new_tab_page::mojom::AccessTokenPtr));
  MOCK_METHOD0(SetAuthStateError, void());
};

std::unique_ptr<TestingProfile> MakeTestingProfile() {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      MicrosoftAuthServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<testing::NiceMock<MockMicrosoftAuthService>>();
      }));
  return profile_builder.Build();
}

}  // namespace

class NtpMicrosoftAuthUntrustedPageHandlerTest : public testing::Test {
 public:
  NtpMicrosoftAuthUntrustedPageHandlerTest()
      : profile_(MakeTestingProfile()),
        mock_auth_service_(static_cast<MockMicrosoftAuthService*>(
            MicrosoftAuthServiceFactory::GetForProfile(profile_.get()))) {}

  ~NtpMicrosoftAuthUntrustedPageHandlerTest() override = default;

  void SetUp() override {
    handler_ = std::make_unique<MicrosoftAuthUntrustedPageHandler>(
        mojo::PendingReceiver<
            new_tab_page::mojom::MicrosoftAuthUntrustedPageHandler>(),
        profile_.get());
  }

  MicrosoftAuthUntrustedPageHandler& handler() { return *handler_; }
  MockMicrosoftAuthService& mock_auth_service() { return *mock_auth_service_; }

 private:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<MicrosoftAuthUntrustedPageHandler> handler_;
  raw_ptr<MockMicrosoftAuthService> mock_auth_service_;
};

TEST_F(NtpMicrosoftAuthUntrustedPageHandlerTest, SetAccessToken) {
  new_tab_page::mojom::AccessTokenPtr access_token_arg;
  EXPECT_CALL(mock_auth_service(), SetAccessToken)
      .WillOnce(MoveArg<0>(&access_token_arg));

  new_tab_page::mojom::AccessTokenPtr access_token =
      new_tab_page::mojom::AccessToken::New();
  access_token->token = "1234";
  access_token->expiration = base::Time::Now();
  handler().SetAccessToken(std::move(access_token));

  ASSERT_TRUE(access_token_arg);
  EXPECT_EQ(access_token_arg->token, "1234");
  EXPECT_EQ(access_token_arg->expiration, base::Time::Now());
}

TEST_F(NtpMicrosoftAuthUntrustedPageHandlerTest, SetAuthStateError) {
  EXPECT_CALL(mock_auth_service(), SetAuthStateError);

  handler().SetAuthStateError();
}
