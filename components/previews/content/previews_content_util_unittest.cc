// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_content_util.h"

#include <memory>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/test/scoped_feature_list.h"
#include "components/previews/content/previews_user_data.h"
#include "content/public/common/previews_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {

// A test implementation of PreviewsDecider that simply returns whether the
// preview type feature is enabled (ignores ECT and blacklist considerations).
class PreviewEnabledPreviewsDecider : public PreviewsDecider {
 public:
  PreviewEnabledPreviewsDecider() {}
  ~PreviewEnabledPreviewsDecider() override {}

  bool ShouldAllowPreviewAtECT(
      PreviewsUserData* previews_data,
      const GURL& url,
      bool is_reload,
      PreviewsType type,
      net::EffectiveConnectionType effective_connection_type_threshold,
      const std::vector<std::string>& host_blacklist_from_finch,
      bool is_server_preview) const override {
    return IsEnabled(type);
  }

  bool ShouldAllowPreview(PreviewsUserData* previews_data,
                          const GURL& url,
                          bool is_reload,
                          PreviewsType type) const override {
    return ShouldAllowPreviewAtECT(previews_data, url, is_reload, type,
                                   params::GetECTThresholdForPreview(type),
                                   std::vector<std::string>(), false);
  }

  bool IsURLAllowedForPreview(PreviewsUserData* previews_data,
                              const GURL& url,
                              PreviewsType type) const override {
    EXPECT_TRUE(type == PreviewsType::NOSCRIPT ||
                type == PreviewsType::RESOURCE_LOADING_HINTS);
    return IsEnabled(type);
  }

  void LoadResourceHints(const GURL& url) override {}

  void LogHintCacheMatch(const GURL& url, bool is_committed) const override {}

 private:
  bool IsEnabled(PreviewsType type) const {
    switch (type) {
      case previews::PreviewsType::OFFLINE:
        return params::IsOfflinePreviewsEnabled();
      case previews::PreviewsType::LOFI:
        return params::IsClientLoFiEnabled();
      case previews::PreviewsType::DEPRECATED_AMP_REDIRECTION:
        return false;
      case previews::PreviewsType::NOSCRIPT:
        return params::IsNoScriptPreviewsEnabled();
      case previews::PreviewsType::RESOURCE_LOADING_HINTS:
        return params::IsResourceLoadingHintsEnabled();
      case previews::PreviewsType::LITE_PAGE_REDIRECT:
        return params::IsLitePageServerPreviewsEnabled();
      case PreviewsType::LITE_PAGE:
      case PreviewsType::NONE:
      case PreviewsType::UNSPECIFIED:
      case PreviewsType::LAST:
        break;
    }
    NOTREACHED();
    return false;
  }
};

class PreviewsContentUtilTest : public testing::Test {
 public:
  PreviewsContentUtilTest() {}
  ~PreviewsContentUtilTest() override {}

  PreviewsDecider* enabled_previews_decider() {
    return &enabled_previews_decider_;
  }

 protected:
  base::MessageLoopForIO loop_;

 private:
  PreviewEnabledPreviewsDecider enabled_previews_decider_;
};

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStatePreviewsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "ClientLoFi,ResourceLoadingHints,NoScriptPreviews" /* enable_features */,
      "Previews" /* disable_features */);
  PreviewsUserData user_data(1);
  EXPECT_EQ(content::PREVIEWS_UNSPECIFIED,
            previews::DetermineAllowedClientPreviewsState(
                &user_data, GURL("http://www.google.com"), false, true,
                enabled_previews_decider()));
  EXPECT_EQ(content::PREVIEWS_UNSPECIFIED,
            previews::DetermineAllowedClientPreviewsState(
                &user_data, GURL("http://www.google.com"), false, true,
                enabled_previews_decider()));
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateDataSaverDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "Previews,ClientLoFi,ResourceLoadingHints,NoScriptPreviews",
      {} /* disable_features */);
  PreviewsUserData user_data(1);
  EXPECT_EQ(content::OFFLINE_PAGE_ON | content::CLIENT_LOFI_ON |
                content::RESOURCE_LOADING_HINTS_ON | content::NOSCRIPT_ON,
            previews::DetermineAllowedClientPreviewsState(
                &user_data, GURL("http://www.google.com"), false, true,
                enabled_previews_decider()));
  EXPECT_EQ(content::OFFLINE_PAGE_ON,
            previews::DetermineAllowedClientPreviewsState(
                &user_data, GURL("http://www.google.com"), false, false,
                enabled_previews_decider()));
}

TEST_F(PreviewsContentUtilTest, DetermineAllowedClientPreviewsStateClientLoFi) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("Previews,ClientLoFi", std::string());
  PreviewsUserData user_data(1);
  EXPECT_TRUE(content::CLIENT_LOFI_ON &
              previews::DetermineAllowedClientPreviewsState(
                  &user_data, GURL("https://www.google.com"), false, true,
                  enabled_previews_decider()));
  EXPECT_TRUE(content::CLIENT_LOFI_ON &
              previews::DetermineAllowedClientPreviewsState(
                  &user_data, GURL("http://www.google.com"), false, true,
                  enabled_previews_decider()));
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateResourceLoadingHints) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("Previews,ResourceLoadingHints",
                                          std::string());
  PreviewsUserData user_data(1);
  EXPECT_LT(0, content::RESOURCE_LOADING_HINTS_ON &
                   previews::DetermineAllowedClientPreviewsState(
                       &user_data, GURL("https://www.google.com"), false, true,
                       enabled_previews_decider()));
  EXPECT_LT(0, content::RESOURCE_LOADING_HINTS_ON &
                   previews::DetermineAllowedClientPreviewsState(
                       &user_data, GURL("http://www.google.com"), false, true,
                       enabled_previews_decider()));
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateNoScriptAndClientLoFi) {
  // Enable both Client LoFi and NoScript.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "Previews,ClientLoFi,NoScriptPreviews", std::string());

  PreviewsUserData user_data(1);
  // Verify both are enabled.
  EXPECT_TRUE((content::NOSCRIPT_ON | content::CLIENT_LOFI_ON) &
              previews::DetermineAllowedClientPreviewsState(
                  &user_data, GURL("https://www.google.com"), false, true,
                  enabled_previews_decider()));
  EXPECT_TRUE((content::NOSCRIPT_ON | content::CLIENT_LOFI_ON) &
              previews::DetermineAllowedClientPreviewsState(
                  &user_data, GURL("http://www.google.com"), false, true,
                  enabled_previews_decider()));

  // Verify non-HTTP[S] URL has no previews enabled.
  EXPECT_EQ(content::PREVIEWS_UNSPECIFIED,
            previews::DetermineAllowedClientPreviewsState(
                &user_data, GURL("data://someblob"), false, true,
                enabled_previews_decider()));
}

TEST_F(PreviewsContentUtilTest, DetermineCommittedClientPreviewsState) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "Previews,ClientLoFi,NoScriptPreviews,ResourceLoadingHints",
      std::string());
  PreviewsUserData user_data(1);
  // Server bits take precedence over NoScript:
  EXPECT_EQ(content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                content::CLIENT_LOFI_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                    content::CLIENT_LOFI_ON | content::NOSCRIPT_ON,
                enabled_previews_decider()));

  // NoScript has precedence over Client LoFi - kept for committed HTTPS:
  EXPECT_EQ(content::NOSCRIPT_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::CLIENT_LOFI_ON | content::NOSCRIPT_ON,
                enabled_previews_decider()));

  // RESOURCE_LOADING_HINTS has precedence over Client LoFi and NoScript.
  EXPECT_EQ(content::RESOURCE_LOADING_HINTS_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::CLIENT_LOFI_ON | content::NOSCRIPT_ON |
                    content::RESOURCE_LOADING_HINTS_ON,
                enabled_previews_decider()));

  // NoScript has precedence over Client LoFi - dropped for committed HTTP:
  EXPECT_EQ(content::PREVIEWS_OFF,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("http://www.google.com"),
                content::CLIENT_LOFI_ON | content::NOSCRIPT_ON |
                    content::RESOURCE_LOADING_HINTS_ON,
                enabled_previews_decider()));

  // Only Client LoFi:
  EXPECT_EQ(content::CLIENT_LOFI_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::CLIENT_LOFI_ON, enabled_previews_decider()));

  // Only NoScript:
  EXPECT_EQ(content::NOSCRIPT_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::NOSCRIPT_ON, enabled_previews_decider()));
}

TEST_F(PreviewsContentUtilTest,
       DetermineCommittedClientPreviewsStateNoScriptCheckIfStillAllowed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("Previews,ClientLoFi",
                                          "NoScriptPreviews");
  PreviewsUserData user_data(1);
  // NoScript not allowed at commit time so Client LoFi chosen:
  EXPECT_EQ(content::PREVIEWS_OFF,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::CLIENT_LOFI_ON | content::NOSCRIPT_ON |
                    content::RESOURCE_LOADING_HINTS_ON,
                enabled_previews_decider()));
}

TEST_F(PreviewsContentUtilTest, GetMainFramePreviewsType) {
  // Simple cases:
  EXPECT_EQ(previews::PreviewsType::LITE_PAGE,
            previews::GetMainFramePreviewsType(content::SERVER_LITE_PAGE_ON));
  EXPECT_EQ(previews::PreviewsType::LOFI,
            previews::GetMainFramePreviewsType(content::SERVER_LOFI_ON));
  EXPECT_EQ(previews::PreviewsType::NOSCRIPT,
            previews::GetMainFramePreviewsType(content::NOSCRIPT_ON));
  EXPECT_EQ(
      previews::PreviewsType::RESOURCE_LOADING_HINTS,
      previews::GetMainFramePreviewsType(content::RESOURCE_LOADING_HINTS_ON));
  EXPECT_EQ(previews::PreviewsType::LOFI,
            previews::GetMainFramePreviewsType(content::CLIENT_LOFI_ON));

  // NONE cases:
  EXPECT_EQ(previews::PreviewsType::NONE,
            previews::GetMainFramePreviewsType(content::PREVIEWS_UNSPECIFIED));
  EXPECT_EQ(previews::PreviewsType::NONE,
            previews::GetMainFramePreviewsType(content::PREVIEWS_NO_TRANSFORM));

  // Precedence cases when server preview is available:
  EXPECT_EQ(previews::PreviewsType::LITE_PAGE,
            previews::GetMainFramePreviewsType(
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                content::NOSCRIPT_ON | content::CLIENT_LOFI_ON |
                content::RESOURCE_LOADING_HINTS_ON));
  EXPECT_EQ(previews::PreviewsType::LOFI,
            previews::GetMainFramePreviewsType(
                content::SERVER_LOFI_ON | content::NOSCRIPT_ON |
                content::CLIENT_LOFI_ON | content::RESOURCE_LOADING_HINTS_ON));

  // Precedence cases when server preview is not available:
  EXPECT_EQ(previews::PreviewsType::NOSCRIPT,
            previews::GetMainFramePreviewsType(content::NOSCRIPT_ON |
                                               content::CLIENT_LOFI_ON));
  EXPECT_EQ(previews::PreviewsType::RESOURCE_LOADING_HINTS,
            previews::GetMainFramePreviewsType(
                content::NOSCRIPT_ON | content::CLIENT_LOFI_ON |
                content::RESOURCE_LOADING_HINTS_ON));
}

}  // namespace

}  // namespace previews
