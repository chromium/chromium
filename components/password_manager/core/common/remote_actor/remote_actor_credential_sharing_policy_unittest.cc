// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/remote_actor/remote_actor_credential_sharing_policy.h"

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/common/remote_actor/remote_actor_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {
namespace {

class RemoteActorCredentialSharingPolicyTest : public testing::Test {
 public:
  RemoteActorCredentialSharingPolicyTest() {
    feature_list_.InitAndEnableFeature(features::kRemoteActorCredentialSharing);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(RemoteActorCredentialSharingPolicyTest,
       ReturnsNulloptWhenFeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      features::kRemoteActorCredentialSharing);

  for (const char* url :
       {"https://gemini.google.com", "https://search.google.com"}) {
    const url::Origin origin = url::Origin::Create(GURL(url));
    SCOPED_TRACE(origin);
    EXPECT_EQ(GetRemoteActorTypeForOrigin(origin), std::nullopt);
    EXPECT_FALSE(IsRemoteActorCredentialSharingAllowedForOrigin(origin));
  }
}

TEST_F(RemoteActorCredentialSharingPolicyTest,
       DefaultOriginsAndRejectsUnlistedOrNonHttps) {
  EXPECT_EQ(GetRemoteActorKey(RemoteActorType::kGemini), "gemini");
  EXPECT_EQ(GetRemoteActorKey(RemoteActorType::kSearch), "search");

  for (const char* url : {
           "https://gemini.google.com",
           "https://gemini.corp.google.com",
           "https://gemini-preprod.corp.google.com",
           "https://gemini-staging.corp.google.com",
           "https://gemini-autopush.corp.google.com",
       }) {
    const url::Origin origin = url::Origin::Create(GURL(url));
    SCOPED_TRACE(origin);
    EXPECT_EQ(GetRemoteActorTypeForOrigin(origin), RemoteActorType::kGemini);
    EXPECT_TRUE(IsRemoteActorCredentialSharingAllowedForOrigin(origin));
  }

  const url::Origin search_origin =
      url::Origin::Create(GURL("https://search.google.com"));
  EXPECT_EQ(GetRemoteActorTypeForOrigin(search_origin),
            RemoteActorType::kSearch);
  EXPECT_TRUE(IsRemoteActorCredentialSharingAllowedForOrigin(search_origin));

  const url::Origin disallowed_origins[] = {
      url::Origin(),
      url::Origin::Create(GURL("https://gemini.google.com"))
          .DeriveNewOpaqueOrigin(),
      url::Origin::Create(GURL("http://gemini.google.com")),
      url::Origin::Create(GURL("http://search.google.com")),
      url::Origin::Create(GURL("https://www.google.com")),
      url::Origin::Create(GURL("https://example.com")),
      url::Origin::Create(GURL("https://gemini.google.com:8443")),
  };
  for (const auto& origin : disallowed_origins) {
    SCOPED_TRACE(origin);
    EXPECT_EQ(GetRemoteActorTypeForOrigin(origin), std::nullopt);
    EXPECT_FALSE(IsRemoteActorCredentialSharingAllowedForOrigin(origin));
  }
}

TEST_F(RemoteActorCredentialSharingPolicyTest,
       AllowedHostForTestingMapsToGeminiOrSearch) {
  const url::Origin localhost_origin =
      url::Origin::Create(GURL("https://localhost:8443"));

  constexpr struct {
    const char* actor_param;
    RemoteActorType expected_type;
  } kCases[] = {
      {"", RemoteActorType::kGemini},
      {"search", RemoteActorType::kSearch},
      {"SEARCH", RemoteActorType::kSearch},
      {"unknown", RemoteActorType::kGemini},
  };

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.actor_param);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kRemoteActorCredentialSharing,
        {{"allowed_host_for_testing", "localhost"},
         {"allowed_host_actor_for_testing", test_case.actor_param}});
    EXPECT_EQ(GetRemoteActorTypeForOrigin(localhost_origin),
              test_case.expected_type);
    EXPECT_EQ(GetRemoteActorTypeForOrigin(
                  url::Origin::Create(GURL("http://localhost:8080"))),
              std::nullopt);
  }
}

TEST_F(RemoteActorCredentialSharingPolicyTest, AdditionalAllowedHostsViaFinch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kRemoteActorCredentialSharing,
      {{"actor_configs_json", R"({
          "gemini": {
            "allowed_hosts": [
              "gemini-canary.corp.google.com",
              "https://gemini-dev.corp.google.com"
            ]
          },
          "search": {
            "allowed_hosts": [
              "www.google.com",
              "/example.com",
              "\\example.com",
              "https:///example.com",
              "https://path.example.com/signin",
              "https://query.example.com?q=1",
              "https://frag.example.com#section",
              "https://user:pass@auth.example.com",
              "http://insecure.example.com",
              "",
              "   ",
              123,
              "  https://search-autopush.corp.google.com  "
            ]
          }
        })"}});

  // Compiled-in defaults remain active alongside Finch `allowed_hosts`.
  EXPECT_EQ(GetRemoteActorTypeForOrigin(
                url::Origin::Create(GURL("https://gemini.google.com"))),
            RemoteActorType::kGemini);
  EXPECT_EQ(GetRemoteActorTypeForOrigin(
                url::Origin::Create(GURL("https://search.google.com"))),
            RemoteActorType::kSearch);

  EXPECT_EQ(GetRemoteActorTypeForOrigin(url::Origin::Create(
                GURL("https://gemini-canary.corp.google.com"))),
            RemoteActorType::kGemini);
  EXPECT_EQ(GetRemoteActorTypeForOrigin(url::Origin::Create(
                GURL("https://gemini-dev.corp.google.com"))),
            RemoteActorType::kGemini);
  EXPECT_EQ(GetRemoteActorTypeForOrigin(
                url::Origin::Create(GURL("https://www.google.com"))),
            RemoteActorType::kSearch);
  EXPECT_EQ(GetRemoteActorTypeForOrigin(url::Origin::Create(
                GURL("https://search-autopush.corp.google.com"))),
            RemoteActorType::kSearch);

  for (const char* rejected_url : {
           "https://gemini-canary.corp.google.com:8443",
           "https://example.com",
           "https://path.example.com",
           "https://query.example.com",
           "https://frag.example.com",
           "https://auth.example.com",
           "https://insecure.example.com",
       }) {
    const url::Origin origin = url::Origin::Create(GURL(rejected_url));
    SCOPED_TRACE(origin);
    EXPECT_EQ(GetRemoteActorTypeForOrigin(origin), std::nullopt);
  }
}

TEST_F(RemoteActorCredentialSharingPolicyTest,
       OriginPrecedenceTestingOverFinchOverDefaults) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kRemoteActorCredentialSharing,
      {{"allowed_host_for_testing", "www.google.com"},
       {"allowed_host_actor_for_testing", "gemini"},
       {"actor_configs_json", R"({
          "gemini": {
            "allowed_hosts": [
              "https://shared.example.com"
            ]
          },
          "search": {
            "allowed_hosts": [
              "https://www.google.com",
              "https://gemini.google.com",
              "https://shared.example.com"
            ]
          }
        })"}});

  // `allowed_host_for_testing` takes precedence over `actor_configs_json`.
  EXPECT_EQ(GetRemoteActorTypeForOrigin(
                url::Origin::Create(GURL("https://www.google.com"))),
            RemoteActorType::kGemini);
  // `actor_configs_json` takes precedence over compiled-in defaults.
  EXPECT_EQ(GetRemoteActorTypeForOrigin(
                url::Origin::Create(GURL("https://gemini.google.com"))),
            RemoteActorType::kSearch);
  // Duplicate origin across actors in `actor_configs_json` resolves in
  // `kAllRemoteActorTypes` order (`kGemini` before `kSearch`).
  EXPECT_EQ(GetRemoteActorTypeForOrigin(
                url::Origin::Create(GURL("https://shared.example.com"))),
            RemoteActorType::kGemini);
}

TEST_F(RemoteActorCredentialSharingPolicyTest,
       InvalidFinchAllowedHostsFallBackToDefaults) {
  const url::Origin example_origin =
      url::Origin::Create(GURL("https://example.com"));

  for (const char* invalid_json : {
           "",
           "{not valid json",
           "[]",
           R"({"gemini": {"allowed_hosts": "https://example.com"}})",
           R"({"gemini": {"allowed_hosts": [123]}})",
       }) {
    SCOPED_TRACE(invalid_json);
    {
      base::test::ScopedFeatureList valid_feature_list;
      valid_feature_list.InitAndEnableFeatureWithParameters(
          features::kRemoteActorCredentialSharing,
          {{"actor_configs_json",
            R"({"gemini": {"allowed_hosts": ["https://example.com"]}})"}});
      EXPECT_EQ(GetRemoteActorTypeForOrigin(example_origin),
                RemoteActorType::kGemini);
    }

    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kRemoteActorCredentialSharing,
        {{"actor_configs_json", invalid_json}});

    EXPECT_EQ(GetRemoteActorTypeForOrigin(
                  url::Origin::Create(GURL("https://gemini.google.com"))),
              RemoteActorType::kGemini);
    EXPECT_EQ(GetRemoteActorTypeForOrigin(example_origin), std::nullopt);
  }
}

}  // namespace
}  // namespace password_manager
