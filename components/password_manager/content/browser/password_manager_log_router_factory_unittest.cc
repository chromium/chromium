// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/password_manager_log_router_factory.h"

#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::LogRouter;
using password_manager::PasswordManagerLogRouterFactory;

namespace {

const char kTestText[] = "abcd1234";

class MockLogReceiver : public autofill::LogReceiver {
 public:
  MOCK_METHOD1(LogEntry, void(const base::Value::Dict&));
};

}  // namespace

class PasswordManagerLogRouterFactoryTest : public testing::Test {
 public:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;

  void SetUp() override {
    BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(
        &browser_context_);
  }

  void TearDown() override {
    BrowserContextDependencyManager::GetInstance()
        ->DestroyBrowserContextServices(&browser_context_);
  }
};

// When the profile is not incognito, it should be possible to activate the
// service.
TEST_F(PasswordManagerLogRouterFactoryTest, ServiceActiveNonIncognito) {
  browser_context_.set_is_off_the_record(false);
  LogRouter* log_router =
      PasswordManagerLogRouterFactory::GetForBrowserContext(&browser_context_);
  testing::StrictMock<MockLogReceiver> receiver;

  ASSERT_TRUE(log_router);
  log_router->RegisterReceiver(&receiver);

  base::Value::Dict log_entry =
      autofill::LogRouter::CreateEntryForText(kTestText);
  EXPECT_CALL(receiver, LogEntry(testing::Eq(testing::ByRef(log_entry))))
      .Times(1);
  log_router->ProcessLog(kTestText);

  log_router->UnregisterReceiver(&receiver);
}

// When the browser profile is incognito, it should not be possible to activate
// the service.
TEST_F(PasswordManagerLogRouterFactoryTest, ServiceNotActiveIncognito) {
  browser_context_.set_is_off_the_record(true);
  LogRouter* log_router =
      PasswordManagerLogRouterFactory::GetForBrowserContext(&browser_context_);
  // BrowserContextKeyedServiceFactory::GetBrowserContextToUse should return
  // nullptr for |browser_context|, because |browser_context| is incognito.
  // Therefore the returned |service| should also be nullptr.
  EXPECT_FALSE(log_router);
}
