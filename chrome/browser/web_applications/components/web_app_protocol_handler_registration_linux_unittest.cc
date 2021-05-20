// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_protocol_handler_registration.h"

#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kApp1Id[] = "app1_id";
const char kApp1Name[] = "app1 name";
const char kApp1Url[] = "https://app1.com/%s";
const char kApp2Id[] = "app2_id";
const char kApp2Name[] = "app2 name";
const char kApp2Url[] = "https://app2.com/%s";

ProtocolHandler GetProtocolHandler(
    const apps::ProtocolHandlerInfo& handler_info,
    const std::string& app_id) {
  return ProtocolHandler::CreateWebAppProtocolHandler(handler_info.protocol,
                                                      handler_info.url, app_id);
}

std::unique_ptr<KeyedService> BuildProtocolHandlerRegistry(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ProtocolHandlerRegistry>(
      profile, std::make_unique<ProtocolHandlerRegistry::Delegate>());
}

}  // namespace

namespace web_app {

class WebAppProtocolHandlerRegistrationLinuxTest : public testing::Test {
 protected:
  WebAppProtocolHandlerRegistrationLinuxTest() = default;

  void SetUp() override {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    profile_ =
        testing_profile_manager_->CreateTestingProfile(chrome::kInitialProfile);

    ProtocolHandlerRegistryFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&BuildProtocolHandlerRegistry));
  }

  void TearDown() override {
    profile_ = nullptr;
    testing_profile_manager_->DeleteAllTestingProfiles();
  }

  Profile* GetProfile() { return profile_; }

  ProtocolHandlerRegistry* protocol_handler_registry() {
    return ProtocolHandlerRegistryFactory::GetForBrowserContext(GetProfile());
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  TestingProfile* profile_;
};

TEST_F(WebAppProtocolHandlerRegistrationLinuxTest, RegisterHandlers) {
  apps::ProtocolHandlerInfo handler1_info;
  handler1_info.protocol = "mailto";
  handler1_info.url = GURL(kApp1Url);
  auto handler1 = GetProtocolHandler(handler1_info, kApp1Id);

  apps::ProtocolHandlerInfo handler2_info;
  handler2_info.protocol = "web+test";
  handler2_info.url = GURL(kApp1Url);
  auto handler2 = GetProtocolHandler(handler2_info, kApp1Id);

  RegisterProtocolHandlersWithOs(kApp1Id, kApp1Name, GetProfile(),
                                 {handler1_info, handler2_info},
                                 base::DoNothing());

  EXPECT_TRUE(protocol_handler_registry()->IsRegistered(handler1));
  EXPECT_TRUE(protocol_handler_registry()->IsDefault(handler1));

  EXPECT_TRUE(protocol_handler_registry()->IsRegistered(handler2));
  EXPECT_TRUE(protocol_handler_registry()->IsDefault(handler2));
}

TEST_F(WebAppProtocolHandlerRegistrationLinuxTest,
       RegisterMultipleHandlersWithSameScheme) {
  apps::ProtocolHandlerInfo handler1_info;
  handler1_info.protocol = "mailto";
  handler1_info.url = GURL(kApp1Url);
  auto handler1 = GetProtocolHandler(handler1_info, kApp1Id);

  RegisterProtocolHandlersWithOs(kApp1Id, kApp1Name, GetProfile(),
                                 {handler1_info}, base::DoNothing());

  apps::ProtocolHandlerInfo handler2_info;
  handler2_info.protocol = "mailto";
  handler2_info.url = GURL(kApp2Url);
  auto handler2 = GetProtocolHandler(handler2_info, kApp2Id);

  RegisterProtocolHandlersWithOs(kApp2Id, kApp2Name, GetProfile(),
                                 {handler2_info}, base::DoNothing());

  EXPECT_TRUE(protocol_handler_registry()->IsRegistered(handler1));
  EXPECT_TRUE(protocol_handler_registry()->IsDefault(handler1));

  EXPECT_TRUE(protocol_handler_registry()->IsRegistered(handler2));
  EXPECT_FALSE(protocol_handler_registry()->IsDefault(handler2));
}

TEST_F(WebAppProtocolHandlerRegistrationLinuxTest, UnregisterHandler) {
  apps::ProtocolHandlerInfo handler_info;
  handler_info.protocol = "mailto";
  handler_info.url = GURL(kApp1Url);
  auto handler = GetProtocolHandler(handler_info, kApp1Id);

  RegisterProtocolHandlersWithOs(kApp1Id, kApp1Name, GetProfile(),
                                 {handler_info}, base::DoNothing());

  ASSERT_TRUE(protocol_handler_registry()->IsRegistered(handler));
  ASSERT_TRUE(protocol_handler_registry()->IsDefault(handler));

  UnregisterProtocolHandlersWithOs(kApp1Id, GetProfile(), {handler_info});

  EXPECT_FALSE(protocol_handler_registry()->IsRegistered(handler));
  EXPECT_FALSE(protocol_handler_registry()->IsDefault(handler));
}

}  // namespace web_app
