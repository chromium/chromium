// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_logger.h"

#include "components/autofill/core/browser/logging/stub_log_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Return;

const char kSiteOrigin[] = "https://example.com";
const char kFederationOrigin[] = "https://google.com";

class MockLogManager : public autofill::StubLogManager {
 public:
  MockLogManager() = default;
  ~MockLogManager() override = default;

  MOCK_CONST_METHOD1(LogTextMessage, void(const std::string& text));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLogManager);
};

class CredentialManagerLoggerTest : public testing::Test {
 public:
  CredentialManagerLoggerTest();
  ~CredentialManagerLoggerTest() override;

  MockLogManager& log_manager() { return log_manager_; }
  CredentialManagerLogger& logger() { return logger_; }

 private:
  MockLogManager log_manager_;
  CredentialManagerLogger logger_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerLoggerTest);
};

CredentialManagerLoggerTest::CredentialManagerLoggerTest()
    : logger_(&log_manager_) {}

CredentialManagerLoggerTest::~CredentialManagerLoggerTest() = default;

TEST_F(CredentialManagerLoggerTest, LogRequestCredential) {
  EXPECT_CALL(log_manager(),
              LogTextMessage(
                  AllOf(HasSubstr(kSiteOrigin), HasSubstr(kFederationOrigin))));
  logger().LogRequestCredential(GURL(kSiteOrigin),
                                CredentialMediationRequirement::kSilent,
                                {GURL(kFederationOrigin)});
}

TEST_F(CredentialManagerLoggerTest, LogSendCredential) {
  EXPECT_CALL(log_manager(), LogTextMessage(HasSubstr(kSiteOrigin)));
  logger().LogSendCredential(GURL(kSiteOrigin),
                             CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(CredentialManagerLoggerTest, LogStoreCredential) {
  EXPECT_CALL(log_manager(), LogTextMessage(HasSubstr(kSiteOrigin)));
  logger().LogStoreCredential(GURL(kSiteOrigin),
                              CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(CredentialManagerLoggerTest, LogPreventSilentAccess) {
  EXPECT_CALL(log_manager(), LogTextMessage(HasSubstr(kSiteOrigin)));
  logger().LogPreventSilentAccess(GURL(kSiteOrigin));
}

}  // namespace
}  // namespace password_manager
