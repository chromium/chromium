// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include <string>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_command_line.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "content/public/test/test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

namespace chrome_common {

TEST(ChromeContentClientTest, AdditionalSchemes) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_TRUE(url::IsStandard(
      extensions::kExtensionScheme,
      url::Component(0, strlen(extensions::kExtensionScheme))));

  GURL extension_url(
      "chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef/foo.html");
  url::Origin origin = url::Origin::Create(extension_url);
  EXPECT_EQ("chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef",
            origin.Serialize());
#endif

  // IsUrlPotentiallyTrustworthy assertions test for https://crbug.com/734581.
  constexpr const char* kChromeLayerUrlsRegisteredAsSecure[] = {
    // The schemes below are registered both as secure and no-access.  Product
    // code needs to treat such URLs as trustworthy, even though no-access
    // schemes translate into an opaque origin (which is untrustworthy).
    "chrome-native://newtab/",
    "chrome-error://foo/",
    // The schemes below are registered as secure (but not as no-access).
    "chrome://foo/",
    "chrome-untrusted://foo/",
    "chrome-search://foo/",
    "isolated-app://foo/",
#if BUILDFLAG(ENABLE_EXTENSIONS)
    "chrome-extension://foo/",
#endif
    "devtools://foo/",
  };
  for (const std::string& str : kChromeLayerUrlsRegisteredAsSecure) {
    SCOPED_TRACE(str);
    GURL url(str);
    EXPECT_TRUE(base::Contains(url::GetSecureSchemes(), url.scheme()));
    EXPECT_TRUE(network::IsUrlPotentiallyTrustworthy(url));
  }

  GURL chrome_url(content::GetWebUIURL("dummyurl"));
  EXPECT_TRUE(network::IsUrlPotentiallyTrustworthy(chrome_url));
  EXPECT_TRUE(content::OriginCanAccessServiceWorkers(chrome_url));
  EXPECT_TRUE(
      network::IsOriginPotentiallyTrustworthy(url::Origin::Create(chrome_url)));
}

class OriginTrialInitializationTestThread
    : public base::PlatformThread::Delegate {
 public:
  explicit OriginTrialInitializationTestThread(
      ChromeContentClient* chrome_client)
      : chrome_client_(chrome_client) {}

  OriginTrialInitializationTestThread(
      const OriginTrialInitializationTestThread&) = delete;
  OriginTrialInitializationTestThread& operator=(
      const OriginTrialInitializationTestThread&) = delete;

  void ThreadMain() override { AccessPolicy(chrome_client_, &policy_objects_); }

  // Static helper which can also be called from the main thread.
  static void AccessPolicy(
      ChromeContentClient* content_client,
      std::vector<raw_ptr<blink::OriginTrialPolicy, VectorExperimental>>*
          policy_objects) {
    // Repeatedly access the lazily-created origin trial policy
    for (int i = 0; i < 20; i++) {
      blink::OriginTrialPolicy* policy = content_client->GetOriginTrialPolicy();
      policy_objects->push_back(policy);
      base::PlatformThread::YieldCurrentThread();
    }
  }

  const std::vector<raw_ptr<blink::OriginTrialPolicy, VectorExperimental>>*
  policy_objects() const {
    return &policy_objects_;
  }

 private:
  raw_ptr<ChromeContentClient> chrome_client_;
  std::vector<raw_ptr<blink::OriginTrialPolicy, VectorExperimental>>
      policy_objects_;
};

// Test that the lazy initialization of Origin Trial policy is resistant to
// races with concurrent access. Failures (especially flaky) indicate that the
// race prevention is no longer sufficient.
TEST(ChromeContentClientTest, OriginTrialPolicyConcurrentInitialization) {
  ChromeContentClient content_client;
  std::vector<raw_ptr<blink::OriginTrialPolicy, VectorExperimental>>
      policy_objects;
  OriginTrialInitializationTestThread thread(&content_client);
  base::PlatformThreadHandle handle;

  ASSERT_TRUE(base::PlatformThread::Create(0, &thread, &handle));

  // Repeatedly access the lazily-created origin trial policy
  OriginTrialInitializationTestThread::AccessPolicy(&content_client,
                                                    &policy_objects);

  base::PlatformThread::Join(handle);

  ASSERT_EQ(20UL, policy_objects.size());

  blink::OriginTrialPolicy* first_policy = policy_objects[0];

  const std::vector<raw_ptr<blink::OriginTrialPolicy, VectorExperimental>>*
      all_policy_objects[] = {
          &policy_objects,
          thread.policy_objects(),
      };

  for (const std::vector<raw_ptr<blink::OriginTrialPolicy, VectorExperimental>>*
           thread_policy_objects : all_policy_objects) {
    EXPECT_GE(20UL, thread_policy_objects->size());
    for (blink::OriginTrialPolicy* policy : *(thread_policy_objects)) {
      EXPECT_EQ(first_policy, policy);
    }
  }
}

}  // namespace chrome_common
