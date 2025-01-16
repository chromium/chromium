// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_page_handler.h"

#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class NtpMicrosoftAuthUntrustedPageHandlerTest : public testing::Test {
 public:
  NtpMicrosoftAuthUntrustedPageHandlerTest() = default;

  ~NtpMicrosoftAuthUntrustedPageHandlerTest() override = default;

  void SetUp() override {
    handler_ = std::make_unique<MicrosoftAuthUntrustedPageHandler>(
        mojo::PendingReceiver<
            new_tab_page::mojom::MicrosoftAuthUntrustedPageHandler>());
  }

  MicrosoftAuthUntrustedPageHandler& handler() { return *handler_; }

 private:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<MicrosoftAuthUntrustedPageHandler> handler_;
};

// TODO(crbug.com/386389198): This only tests that it doesn't crash for now.
// Better testing will be added when the auth service is created and connected.
TEST_F(NtpMicrosoftAuthUntrustedPageHandlerTest, SetAccessToken) {
  new_tab_page::mojom::AccessTokenPtr access_token =
      new_tab_page::mojom::AccessToken::New();
  access_token->token = "1234";
  access_token->expiration = base::Time::Now();
  handler().SetAccessToken(std::move(access_token));
}
