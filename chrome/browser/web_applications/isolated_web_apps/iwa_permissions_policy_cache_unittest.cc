// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/iwa_permissions_policy_cache.h"

#include "base/test/gmock_expected_support.h"
#include "base/values.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "content/public/common/content_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {
namespace {

using testing::IsEmpty;
using testing::NotNull;
using testing::UnorderedElementsAre;

MATCHER_P2(IsolatedAppPermissionPolicyEntryIs,
           feature,
           allowed_origins_matcher,
           "") {
  return arg.feature == feature &&
         ExplainMatchResult(
             testing::MatcherCast<const std::vector<std::string>&>(
                 allowed_origins_matcher),
             arg.allowed_origins, result_listener);
}

}  // namespace

class IwaPermissionsPolicyCacheTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
    cache_ = IwaPermissionsPolicyCacheFactory::GetForProfile(profile());
  }

  void TearDown() override {
    cache_ = nullptr;
    WebAppTest::TearDown();
  }

  IwaPermissionsPolicyCache* cache() { return cache_; }

 private:
  raw_ptr<IwaPermissionsPolicyCache> cache_;
#if !BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kIsolatedWebApps};
#endif  // !BUILDFLAG(IS_CHROMEOS)
};

TEST_F(IwaPermissionsPolicyCacheTest, ParseManifestAndSetPolicy_Complex) {
  const IwaOrigin kOrigin =
      IwaOrigin::Create(
          GURL("isolated-app://"
               "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic"))
          .value();
  const std::string kManifest = R"({
    "permissions_policy": {
      "camera": ["self", "https://example.com", "https://other.com"],
      "geolocation": ["*"],
      "microphone": ["none"],
      "fullscreen": [],
      "midi": ["self"],
      "usb": ["https://a.com", "https://b.com", "https://c.com", "https://d.com", "https://e.com"],
      "hid": ["none", "https://example.com"]
    }
  })";

  EXPECT_TRUE(cache()->ParseManifestAndSetPolicy(kOrigin, kManifest));

  const auto* policy = cache()->GetPolicy(kOrigin);
  ASSERT_THAT(policy, NotNull());
  EXPECT_THAT(
      *policy,
      UnorderedElementsAre(
          IsolatedAppPermissionPolicyEntryIs(
              "camera",
              std::vector<std::string>(
                  {"'self'", "https://example.com", "https://other.com"})),
          IsolatedAppPermissionPolicyEntryIs("geolocation",
                                             std::vector<std::string>({"*"})),
          IsolatedAppPermissionPolicyEntryIs(
              "microphone", std::vector<std::string>({"'none'"})),
          IsolatedAppPermissionPolicyEntryIs("fullscreen", IsEmpty()),
          IsolatedAppPermissionPolicyEntryIs(
              "midi", std::vector<std::string>({"'self'"})),
          IsolatedAppPermissionPolicyEntryIs(
              "usb", std::vector<std::string>({"https://a.com", "https://b.com",
                                               "https://c.com", "https://d.com",
                                               "https://e.com"})),
          IsolatedAppPermissionPolicyEntryIs(
              "hid",
              std::vector<std::string>({"'none'", "https://example.com"}))));
}

TEST_F(IwaPermissionsPolicyCacheTest, ParseManifestAndSetPolicy_Valid) {
  const IwaOrigin kOrigin =
      IwaOrigin::Create(
          GURL("isolated-app://"
               "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic"))
          .value();
  const std::string kManifest = R"({
    "permissions_policy": {
      "camera": ["self", "https://example.com"],
      "geolocation": ["*"],
      "microphone": ["none"]
    }
  })";

  EXPECT_TRUE(cache()->ParseManifestAndSetPolicy(kOrigin, kManifest));

  const auto* policy = cache()->GetPolicy(kOrigin);
  ASSERT_THAT(policy, NotNull());
  EXPECT_THAT(*policy,
              UnorderedElementsAre(
                  IsolatedAppPermissionPolicyEntryIs(
                      "camera", std::vector<std::string>(
                                    {"'self'", "https://example.com"})),
                  IsolatedAppPermissionPolicyEntryIs(
                      "geolocation", std::vector<std::string>({"*"})),
                  IsolatedAppPermissionPolicyEntryIs(
                      "microphone", std::vector<std::string>({"'none'"}))));
}

TEST_F(IwaPermissionsPolicyCacheTest, ParseManifestAndSetPolicy_NoPolicy) {
  const IwaOrigin kOrigin =
      IwaOrigin::Create(
          GURL("isolated-app://"
               "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic"))
          .value();
  const std::string kManifest = R"({
    "name": "Test App"
  })";

  EXPECT_TRUE(cache()->ParseManifestAndSetPolicy(kOrigin, kManifest));

  const auto* policy = cache()->GetPolicy(kOrigin);
  ASSERT_THAT(policy, NotNull());
  EXPECT_THAT(*policy, IsEmpty());
}

TEST_F(IwaPermissionsPolicyCacheTest, ParseManifestAndSetPolicy_InvalidJson) {
  const IwaOrigin kOrigin =
      IwaOrigin::Create(
          GURL("isolated-app://"
               "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic"))
          .value();
  const std::string kManifest = "invalid json";

  EXPECT_FALSE(cache()->ParseManifestAndSetPolicy(kOrigin, kManifest));
  EXPECT_THAT(cache()->GetPolicy(kOrigin), testing::IsNull());
}

TEST_F(IwaPermissionsPolicyCacheTest, ParseManifestAndSetPolicy_NotDict) {
  const IwaOrigin kOrigin =
      IwaOrigin::Create(
          GURL("isolated-app://"
               "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic"))
          .value();
  const std::string kManifest = "[]";

  EXPECT_FALSE(cache()->ParseManifestAndSetPolicy(kOrigin, kManifest));
  EXPECT_THAT(cache()->GetPolicy(kOrigin), testing::IsNull());
}

TEST_F(IwaPermissionsPolicyCacheTest,
       ParseManifestAndSetPolicy_InvalidPolicyFormat_NotList) {
  const IwaOrigin kOrigin =
      IwaOrigin::Create(
          GURL("isolated-app://"
               "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic"))
          .value();
  const std::string kManifest = R"({
    "permissions_policy": {
      "camera": "invalid"
    }
  })";

  EXPECT_FALSE(cache()->ParseManifestAndSetPolicy(kOrigin, kManifest));
  EXPECT_THAT(cache()->GetPolicy(kOrigin), testing::IsNull());
}

TEST_F(IwaPermissionsPolicyCacheTest,
       ParseManifestAndSetPolicy_InvalidPolicyFormat_ItemNotString) {
  const IwaOrigin kOrigin =
      IwaOrigin::Create(
          GURL("isolated-app://"
               "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic"))
          .value();
  const std::string kManifest = R"({
    "permissions_policy": {
      "camera": [123]
    }
  })";

  EXPECT_FALSE(cache()->ParseManifestAndSetPolicy(kOrigin, kManifest));
  EXPECT_THAT(cache()->GetPolicy(kOrigin), testing::IsNull());
}

}  // namespace web_app
