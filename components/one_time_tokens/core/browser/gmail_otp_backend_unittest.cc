// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

class GmailOtpBackendImplTest : public testing::Test {
 public:
  GmailOtpBackendImplTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(GmailOtpBackendImplTest, SubscribeAndGetFakeToken) {
  GmailOtpBackendImpl backend;
  std::optional<base::expected<OneTimeToken, OneTimeTokenRetrievalError>>
      result;
  base::RunLoop run_loop;

  ExpiringSubscription subscription = backend.Subscribe(
      base::Time::Now() + base::Minutes(1),
      base::BindRepeating(
          [](std::optional<base::expected<OneTimeToken,
                                          OneTimeTokenRetrievalError>>* result,
             base::RunLoop* run_loop,
             base::expected<OneTimeToken, OneTimeTokenRetrievalError>
                 token_or_error) {
            *result = std::move(token_or_error);
            run_loop->Quit();
          },
          &result, &run_loop));

  run_loop.Run();

  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value().has_value());

  const auto& token = result.value().value();
  EXPECT_EQ(token.type(), OneTimeTokenType::kGmail);
  EXPECT_EQ(token.value(), "123456");
  EXPECT_FALSE(token.on_device_arrival_time().is_null());
}

}  // namespace one_time_tokens
