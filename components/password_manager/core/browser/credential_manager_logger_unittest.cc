// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_logger.h"

#include <string>
#include <string_view>

#include "components/autofill/core/browser/logging/stub_log_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Property;
using ::testing::Return;

constexpr char kSiteOrigin[] = "https://example.com";
constexpr char kFederationOrigin[] = "https://google.com";

auto JsonHasSubstr(std::string_view text) {
  return testing::ResultOf(
      [](const base::Value::Dict& dict) {
        const std::string* value = dict.FindString("value");
        return value ? *value : "";
      },
      HasSubstr(text));
}

class MockLogManager : public autofill::StubLogManager {
 public:
  MockLogManager() = default;
  MockLogManager(const MockLogManager&) = delete;
  MockLogManager& operator=(const MockLogManager&) = delete;
  ~MockLogManager() override = default;

  MOCK_METHOD(void,
              ProcessLog,
              (base::Value::Dict node,
               base::PassKey<autofill::LogBufferSubmitter>),
              (override));
};

class CredentialManagerLoggerTest : public testing::Test {
 public:
  CredentialManagerLoggerTest();
  CredentialManagerLoggerTest(const CredentialManagerLoggerTest&) = delete;
  CredentialManagerLoggerTest& operator=(const CredentialManagerLoggerTest&) =
      delete;
  ~CredentialManagerLoggerTest() override;

  MockLogManager& log_manager() { return log_manager_; }
  CredentialManagerLogger& logger() { return logger_; }

 private:
  MockLogManager log_manager_;
  CredentialManagerLogger logger_;
};

CredentialManagerLoggerTest::CredentialManagerLoggerTest()
    : logger_(&log_manager_) {}

CredentialManagerLoggerTest::~CredentialManagerLoggerTest() = default;

TEST_F(CredentialManagerLoggerTest, LogRequestCredential) {
  EXPECT_CALL(log_manager(), ProcessLog(AllOf(JsonHasSubstr(kSiteOrigin),
                                              JsonHasSubstr(kFederationOrigin)),
                                        _));
  logger().LogRequestCredential(url::Origin::Create(GURL(kSiteOrigin)),
                                CredentialMediationRequirement::kSilent,
                                {GURL(kFederationOrigin)});
}

TEST_F(CredentialManagerLoggerTest, LogSendCredential) {
  EXPECT_CALL(log_manager(), ProcessLog(JsonHasSubstr(kSiteOrigin), _));
  logger().LogSendCredential(url::Origin::Create(GURL(kSiteOrigin)),
                             CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(CredentialManagerLoggerTest, LogStoreCredential) {
  EXPECT_CALL(log_manager(), ProcessLog(JsonHasSubstr(kSiteOrigin), _));
  logger().LogStoreCredential(url::Origin::Create(GURL(kSiteOrigin)),
                              CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(CredentialManagerLoggerTest, LogPreventSilentAccess) {
  EXPECT_CALL(log_manager(), ProcessLog(JsonHasSubstr(kSiteOrigin), _));
  logger().LogPreventSilentAccess(url::Origin::Create(GURL(kSiteOrigin)));
}

}  // namespace
}  // namespace password_manager
