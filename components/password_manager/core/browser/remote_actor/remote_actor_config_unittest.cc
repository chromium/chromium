// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/remote_actor/remote_actor_config.h"

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/common/remote_actor/remote_actor_credential_sharing_policy.h"
#include "components/password_manager/core/common/remote_actor/remote_actor_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {
namespace {

class RemoteActorConfigTest : public testing::Test {
 public:
  RemoteActorConfigTest() {
    feature_list_.InitAndEnableFeature(features::kRemoteActorCredentialSharing);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(RemoteActorConfigTest, DefaultGeminiAndSearchConfigForOrigin) {
  std::optional<RemoteActorConfig> gemini_config =
      GetRemoteActorConfigForOrigin(
          url::Origin::Create(GURL("https://gemini.google.com")));
  ASSERT_TRUE(gemini_config.has_value());
  EXPECT_EQ(gemini_config->actor_type, RemoteActorType::kGemini);
  EXPECT_EQ(gemini_config->display_name, u"Gemini");
  EXPECT_EQ(gemini_config->learn_more_url,
            GURL("https://support.google.com/gemini?p=lm_gpm_data"));
  EXPECT_TRUE(gemini_config->icon_url.is_empty());
  EXPECT_EQ(gemini_config->oauth_client_id,
            "320695880279-gnq6the97ga85scn208u5jctnk82qelk."
            "apps.googleusercontent.com");
  EXPECT_EQ(gemini_config->task_namespace_id, 30);

  std::optional<RemoteActorConfig> search_config =
      GetRemoteActorConfigForOrigin(
          url::Origin::Create(GURL("https://search.google.com")));
  ASSERT_TRUE(search_config.has_value());
  EXPECT_EQ(search_config->actor_type, RemoteActorType::kSearch);
  EXPECT_EQ(search_config->display_name, u"Search Agent");
  EXPECT_EQ(search_config->learn_more_url, GURL("https://g.co/privacypolicy"));
  EXPECT_TRUE(search_config->icon_url.is_empty());
  EXPECT_EQ(search_config->oauth_client_id,
            "240392999166-p989rucds3hg2lidtif5huititvl4ln1."
            "apps.googleusercontent.com");
  EXPECT_EQ(search_config->task_namespace_id, 41);

  EXPECT_EQ(GetRemoteActorConfigForOrigin(
                url::Origin::Create(GURL("https://example.com"))),
            std::nullopt);
}

TEST_F(RemoteActorConfigTest, FinchOverridesAndLegacyParameterPrecedence) {
  // Legacy Gemini params apply to `kGemini`, are ignored by `kSearch`, and are
  // overridden when `actor_configs_json` also specifies values.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kRemoteActorCredentialSharing,
        {{"actor_name_for_testing", "Legacy Test Actor"},
         {"oauth_client_id", "legacy-gemini-client-id"}});

    RemoteActorConfig gemini_config =
        GetRemoteActorConfig(RemoteActorType::kGemini);
    EXPECT_EQ(gemini_config.display_name, u"Legacy Test Actor");
    EXPECT_EQ(gemini_config.oauth_client_id, "legacy-gemini-client-id");

    RemoteActorConfig search_config =
        GetRemoteActorConfig(RemoteActorType::kSearch);
    EXPECT_EQ(search_config.display_name, u"Search Agent");
    EXPECT_EQ(search_config.oauth_client_id,
              "240392999166-p989rucds3hg2lidtif5huititvl4ln1."
              "apps.googleusercontent.com");
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kRemoteActorCredentialSharing,
        {{"actor_name_for_testing", "Legacy Test Actor"},
         {"oauth_client_id", "legacy-gemini-client-id"},
         {"actor_configs_json", R"({
            "gemini": {
              "actor_name": "Custom Gemini",
              "learn_more_url": "https://support.google.com/gemini/custom",
              "icon_url": "https://www.gstatic.com/gemini_custom.png",
              "oauth_client_id": "gemini-custom-client.apps.googleusercontent.com",
              "task_namespace_id": 35
            },
            "search": {
              "allowed_hosts": ["https://www.google.com"],
              "actor_name": "AI Mode",
              "learn_more_url": "https://support.google.com/websearch/custom",
              "icon_url": "https://www.gstatic.com/search_custom.png",
              "oauth_client_id": "716693245890-s29c4h245s5b39r75e6vup8h0914998b.apps.googleusercontent.com",
              "task_namespace_id": 42
            }
          })"}});

    RemoteActorConfig gemini_config =
        GetRemoteActorConfig(RemoteActorType::kGemini);
    EXPECT_EQ(gemini_config.display_name, u"Custom Gemini");
    EXPECT_EQ(gemini_config.learn_more_url,
              GURL("https://support.google.com/gemini/custom"));
    EXPECT_EQ(gemini_config.icon_url,
              GURL("https://www.gstatic.com/gemini_custom.png"));
    EXPECT_EQ(gemini_config.oauth_client_id,
              "gemini-custom-client.apps.googleusercontent.com");
    EXPECT_EQ(gemini_config.task_namespace_id, 35);

    std::optional<RemoteActorConfig> search_config =
        GetRemoteActorConfigForOrigin(
            url::Origin::Create(GURL("https://www.google.com")));
    ASSERT_TRUE(search_config.has_value());
    EXPECT_EQ(search_config->actor_type, RemoteActorType::kSearch);
    EXPECT_EQ(search_config->display_name, u"AI Mode");
    EXPECT_EQ(search_config->learn_more_url,
              GURL("https://support.google.com/websearch/custom"));
    EXPECT_EQ(search_config->icon_url,
              GURL("https://www.gstatic.com/search_custom.png"));
    EXPECT_EQ(search_config->oauth_client_id,
              "716693245890-s29c4h245s5b39r75e6vup8h0914998b."
              "apps.googleusercontent.com");
    EXPECT_EQ(search_config->task_namespace_id, 42);
  }
}

TEST_F(RemoteActorConfigTest, InvalidFinchOverridesFallBackToSafeDefaults) {
  for (const char* invalid_json : {
           "{not valid json",
           "[]",
           R"json({
             "gemini": {
               "actor_name": "",
               "learn_more_url": "http://insecure.example.com",
               "icon_url": "http://insecure.example.com/icon.png",
               "oauth_client_id": "",
               "task_namespace_id": -1
             },
             "search": {
               "actor_name": "",
               "learn_more_url": "javascript:alert(1)",
               "icon_url": "not a url",
               "oauth_client_id": "",
               "task_namespace_id": "35"
             }
           })json",
       }) {
    SCOPED_TRACE(invalid_json);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kRemoteActorCredentialSharing,
        {{"actor_name_for_testing", ""},
         {"oauth_client_id", ""},
         {"actor_configs_json", invalid_json}});

    RemoteActorConfig gemini_config =
        GetRemoteActorConfig(RemoteActorType::kGemini);
    EXPECT_EQ(gemini_config.display_name, u"Gemini");
    EXPECT_EQ(gemini_config.learn_more_url,
              GURL("https://support.google.com/gemini?p=lm_gpm_data"));
    EXPECT_TRUE(gemini_config.icon_url.is_empty());
    EXPECT_EQ(gemini_config.oauth_client_id,
              "320695880279-gnq6the97ga85scn208u5jctnk82qelk."
              "apps.googleusercontent.com");
    EXPECT_EQ(gemini_config.task_namespace_id, 30);

    RemoteActorConfig search_config =
        GetRemoteActorConfig(RemoteActorType::kSearch);
    EXPECT_EQ(search_config.display_name, u"Search Agent");
    EXPECT_EQ(search_config.learn_more_url, GURL("https://g.co/privacypolicy"));
    EXPECT_TRUE(search_config.icon_url.is_empty());
    EXPECT_EQ(search_config.oauth_client_id,
              "240392999166-p989rucds3hg2lidtif5huititvl4ln1."
              "apps.googleusercontent.com");
    EXPECT_EQ(search_config.task_namespace_id, 41);
  }
}

}  // namespace
}  // namespace password_manager
