// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include <string>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_command_line.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "ppapi/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace chrome_common {

#if BUILDFLAG(ENABLE_PLUGINS)
TEST(ChromeContentClientTest, FindMostRecent) {
  std::vector<std::unique_ptr<content::PepperPluginInfo>> version_vector;
  // Test an empty vector.
  EXPECT_EQ(nullptr, ChromeContentClient::FindMostRecentPlugin(version_vector));

  // Now test the vector with one element.
  content::PepperPluginInfo info;
  info.version = "1.0.0.0";
  version_vector.push_back(std::make_unique<content::PepperPluginInfo>(info));

  content::PepperPluginInfo* most_recent =
      ChromeContentClient::FindMostRecentPlugin(version_vector);
  EXPECT_EQ("1.0.0.0", most_recent->version);

  content::PepperPluginInfo info5;
  info5.version = "5.0.12.1";
  content::PepperPluginInfo info6_12;
  info6_12.version = "6.0.0.12";
  content::PepperPluginInfo info6_13;
  info6_13.version = "6.0.0.13";

  // Test highest version is picked.
  version_vector.clear();
  version_vector.push_back(std::make_unique<content::PepperPluginInfo>(info5));
  version_vector.push_back(
      std::make_unique<content::PepperPluginInfo>(info6_12));
  version_vector.push_back(
      std::make_unique<content::PepperPluginInfo>(info6_13));

  most_recent = ChromeContentClient::FindMostRecentPlugin(version_vector);
  EXPECT_EQ("6.0.0.13", most_recent->version);

  // Test that order does not matter, validates tests below.
  version_vector.clear();
  version_vector.push_back(
      std::make_unique<content::PepperPluginInfo>(info6_13));
  version_vector.push_back(
      std::make_unique<content::PepperPluginInfo>(info6_12));
  version_vector.push_back(std::make_unique<content::PepperPluginInfo>(info5));

  most_recent = ChromeContentClient::FindMostRecentPlugin(version_vector);
  EXPECT_EQ("6.0.0.13", most_recent->version);

  // Test real scenarios.
  content::PepperPluginInfo component_flash;
  component_flash.version = "4.3.2.1";
  component_flash.is_external = false;
  component_flash.name = "component_flash";

  content::PepperPluginInfo system_flash;
  system_flash.version = "4.3.2.1";
  system_flash.is_external = true;
  system_flash.name = "system_flash";

  // The order here should be:
  // 1. System Flash.
  // 2. Component update.
  version_vector.clear();
  version_vector.push_back(
      std::make_unique<content::PepperPluginInfo>(system_flash));
  version_vector.push_back(
      std::make_unique<content::PepperPluginInfo>(component_flash));
  most_recent = ChromeContentClient::FindMostRecentPlugin(version_vector);
  EXPECT_STREQ("system_flash", most_recent->name.c_str());
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

TEST(ChromeContentClientTest, AdditionalSchemes) {
  EXPECT_TRUE(url::IsStandard(
      extensions::kExtensionScheme,
      url::Component(0, strlen(extensions::kExtensionScheme))));

  GURL extension_url(
      "chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef/foo.html");
  url::Origin origin = url::Origin::Create(extension_url);
  EXPECT_EQ("chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef",
            origin.Serialize());

  EXPECT_TRUE(content::IsOriginSecure(GURL("chrome-native://newtab/")));

  GURL chrome_url(content::GetWebUIURL("dummyurl"));
  EXPECT_TRUE(content::IsOriginSecure(chrome_url));
  EXPECT_FALSE(content::OriginCanAccessServiceWorkers(chrome_url));
  EXPECT_TRUE(
      content::IsPotentiallyTrustworthyOrigin(url::Origin::Create(chrome_url)));
}

class OriginTrialInitializationTestThread
    : public base::PlatformThread::Delegate {
 public:
  explicit OriginTrialInitializationTestThread(
      ChromeContentClient* chrome_client)
      : chrome_client_(chrome_client) {}

  void ThreadMain() override { AccessPolicy(chrome_client_, &policy_objects_); }

  // Static helper which can also be called from the main thread.
  static void AccessPolicy(
      ChromeContentClient* content_client,
      std::vector<blink::OriginTrialPolicy*>* policy_objects) {
    // Repeatedly access the lazily-created origin trial policy
    for (int i = 0; i < 20; i++) {
      blink::OriginTrialPolicy* policy = content_client->GetOriginTrialPolicy();
      policy_objects->push_back(policy);
      base::PlatformThread::YieldCurrentThread();
    }
  }

  const std::vector<blink::OriginTrialPolicy*>* policy_objects() const {
    return &policy_objects_;
  }

 private:
  ChromeContentClient* chrome_client_;
  std::vector<blink::OriginTrialPolicy*> policy_objects_;

  DISALLOW_COPY_AND_ASSIGN(OriginTrialInitializationTestThread);
};

// Test that the lazy initialization of Origin Trial policy is resistant to
// races with concurrent access. Failures (especially flaky) indicate that the
// race prevention is no longer sufficient.
TEST(ChromeContentClientTest, OriginTrialPolicyConcurrentInitialization) {
  ChromeContentClient content_client;
  std::vector<blink::OriginTrialPolicy*> policy_objects;
  OriginTrialInitializationTestThread thread(&content_client);
  base::PlatformThreadHandle handle;

  ASSERT_TRUE(base::PlatformThread::Create(0, &thread, &handle));

  // Repeatedly access the lazily-created origin trial policy
  OriginTrialInitializationTestThread::AccessPolicy(&content_client,
                                                    &policy_objects);

  base::PlatformThread::Join(handle);

  ASSERT_EQ(20UL, policy_objects.size());

  blink::OriginTrialPolicy* first_policy = policy_objects[0];

  const std::vector<blink::OriginTrialPolicy*>* all_policy_objects[] = {
      &policy_objects, thread.policy_objects(),
  };

  for (const std::vector<blink::OriginTrialPolicy*>* thread_policy_objects :
       all_policy_objects) {
    EXPECT_GE(20UL, thread_policy_objects->size());
    for (blink::OriginTrialPolicy* policy : *(thread_policy_objects)) {
      EXPECT_EQ(first_policy, policy);
    }
  }
}

}  // namespace chrome_common
