// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/ai_mode_button_service.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/ai_mode_button_config.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/test_ai_mode_button_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class AiModeButtonServiceTest : public testing::Test {
 public:
  void SetUp() override {
    template_url_service()->Load();
    service_ = std::make_unique<AiModeButtonService>(template_url_service());
  }

  TemplateURLService* template_url_service() {
    return search_engines_test_environment_.template_url_service();
  }

  TemplateURL* FindTurl(const std::u16string& keyword) {
    for (const auto& turl : template_url_service()->GetTemplateURLs()) {
      if (turl->keyword() == keyword) {
        return turl.get();
      }
    }
    NOTREACHED();
  }

  base::test::TaskEnvironment task_environment_;
  const std::vector<TemplateURLService::Initializer> test_engines_ = {
      {"google", "https://google.com/search?q={searchTerms}", "Google"},
      {"google2", "https://google.co.uk/search?q={searchTerms}", "Google 2"},
      {"nongoogle", "https://nongoogle.com/search?q={searchTerms}",
       "Non Google"},
      {"nongoogle2", "https://nongoogle.com/search?q={searchTerms}",
       "Non Google 2"},
      {"bing", "https://bing.com/search?q={searchTerms}", "Bing"},
  };
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_{
      {.template_url_service_initializer = test_engines_}};
  std::unique_ptr<AiModeButtonService> service_;
};

TEST_F(AiModeButtonServiceTest, ConfigWithGoogleDse) {
  // Expect google config when DSE is google.
  const auto* config = service_->GetCurrentConfig();
  ASSERT_TRUE(config);
  EXPECT_EQ(config->id, SearchEngineType::SEARCH_ENGINE_GOOGLE);
  EXPECT_EQ(std::u16string_view(config->text), u"AI Mode");
  EXPECT_EQ(std::u16string_view(config->tooltip),
            u"Ask AI Mode in Google Search");
  EXPECT_EQ(std::u16string_view(config->a11y_label),
            u"AI Mode button, press Enter to ask AI Mode");
  // Context menu item capitalization is platform dependent.
  EXPECT_THAT(std::u16string_view(config->context_menu_label),
              testing::AnyOf(u"Always Show AI Mode", u"Always show AI Mode"));
  EXPECT_EQ(std::u16string_view(config->placeholder_text),
            u"Press tab then enter to ask AI Mode");
}

TEST_F(AiModeButtonServiceTest, NoConfigWithNonGoogleDse) {
  // Expect no config when DSE is not google.
  template_url_service()->SetUserSelectedDefaultSearchProvider(
      FindTurl(u"nongoogle"));
  const auto* config = service_->GetCurrentConfig();
  EXPECT_FALSE(config);
}

TEST_F(AiModeButtonServiceTest, CallbackCalledWhenDseChange) {
  base::MockRepeatingCallback<void(
      const ai_mode_button_config::AiModeButtonConfig*)>
      callback;

  // Expect immediate notification on registration.
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([this](const auto* config) {
    EXPECT_TRUE(config);
    EXPECT_EQ(config->id, SearchEngineType::SEARCH_ENGINE_GOOGLE);
    EXPECT_EQ(config, service_->GetCurrentConfig());
  });
  auto subscription = service_->RegisterOnConfigChanged(callback.Get());
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Expect call on DSE change; null config for non-google DSE.
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([this](const auto* config) {
    EXPECT_FALSE(config);
    EXPECT_FALSE(service_->GetCurrentConfig());
  });
  template_url_service()->SetUserSelectedDefaultSearchProvider(
      FindTurl(u"nongoogle"));
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Expect no call on DSE change when config is unaffected.
  EXPECT_CALL(callback, Run(testing::_)).Times(0);
  template_url_service()->SetUserSelectedDefaultSearchProvider(
      FindTurl(u"nongoogle2"));
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Expect call on DSE change back; google config for google DSE.
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([this](const auto* config) {
    EXPECT_TRUE(config);
    EXPECT_EQ(config->id, SearchEngineType::SEARCH_ENGINE_GOOGLE);
    EXPECT_EQ(config, service_->GetCurrentConfig());
  });
  template_url_service()->SetUserSelectedDefaultSearchProvider(
      FindTurl(u"google"));

  // Expect no call on DSE change when config is unaffected.
  EXPECT_CALL(callback, Run(testing::_)).Times(0);
  template_url_service()->SetUserSelectedDefaultSearchProvider(
      FindTurl(u"google2"));
}

TEST_F(AiModeButtonServiceTest, IsValidConfig) {
  // Helper to mutate the const pointers of AiModeButtonConfig.
  auto mutate = []<typename T>(const T& field, T new_value) {
    const_cast<T&>(field) = new_value;
  };

  // Test empty config.
  ai_mode_button_config::AiModeButtonConfig empty_config = {
      .id = SearchEngineType::SEARCH_ENGINE_UNKNOWN,
      .text = nullptr,
      .tooltip = nullptr,
      .a11y_label = nullptr,
      .context_menu_label = nullptr,
      .placeholder_text = nullptr,
      .favicon_url = nullptr,
      .navigation_url = nullptr,
      .navigation_url_empty = nullptr,
  };
  EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(empty_config));

  // Test valid config.
  ai_mode_button_config::AiModeButtonConfig valid_config{
      SearchEngineType::SEARCH_ENGINE_BING,
      u"Bing text",
      u"Bing tooltip",
      u"Bing a11y",
      u"Bing menu",
      u"Bing placeholder",
      "https://bing.com/favicon.ico",
      "https://bing.com/search?q={searchTerms}",
      "https://bing.com/chat",
  };
  EXPECT_TRUE(TestAiModeButtonService::IsValidConfig(valid_config));

  // Test individual fields missing.
  {
    auto config_copy = valid_config;
    mutate(config_copy.text, u"");
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }
  {
    auto config_copy = valid_config;
    mutate(config_copy.tooltip, u"");
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }
  {
    auto config_copy = valid_config;
    mutate(config_copy.a11y_label, u"");
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }
  {
    auto config_copy = valid_config;
    mutate(config_copy.context_menu_label, u"");
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }
  {
    auto config_copy = valid_config;
    mutate(config_copy.placeholder_text, u"");
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }
  {
    auto config_copy = valid_config;
    mutate(config_copy.favicon_url, "");
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }
  {
    auto config_copy = valid_config;
    mutate(config_copy.navigation_url, "");
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }
  {
    auto config_copy = valid_config;
    mutate(config_copy.navigation_url_empty, "");
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }

  // Test string fields too long.
  {
    auto config_copy = valid_config;
    std::u16string long_str(17, 'a');
    mutate(config_copy.text, long_str.c_str());
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }
  {
    auto config_copy = valid_config;
    std::u16string long_str(65, 'a');
    mutate(config_copy.tooltip, long_str.c_str());
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }
  {
    auto config_copy = valid_config;
    std::u16string long_str(65, 'a');
    mutate(config_copy.a11y_label, long_str.c_str());
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }
  {
    auto config_copy = valid_config;
    std::u16string long_str(65, 'a');
    mutate(config_copy.context_menu_label, long_str.c_str());
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }
  {
    auto config_copy = valid_config;
    std::u16string long_str(65, 'a');
    mutate(config_copy.placeholder_text, long_str.c_str());
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }

  // Test invalid urls.
  {
    auto config_copy = valid_config;
    mutate(config_copy.favicon_url, "bad");
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }
  {
    auto config_copy = valid_config;
    mutate(config_copy.navigation_url, "bad");
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }
  {
    auto config_copy = valid_config;
    mutate(config_copy.navigation_url_empty, "bad");
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }

  // Test missing search placeholder in `navigation_url`.
  {
    auto config_copy = valid_config;
    mutate(config_copy.navigation_url,
           "https://bing.com/search?q=no_placeholder");
    EXPECT_FALSE(TestAiModeButtonService::IsValidConfig(config_copy));
  }

  // Test the tests above were actually making copies and not mutating the
  // original.
  {
    auto config_copy = valid_config;
    EXPECT_TRUE(TestAiModeButtonService::IsValidConfig(config_copy));
  }
}

TEST(AiModeButtonConfigTest, AllCompiledThirdPartyConfigsAreValid) {
  // Verify that every single 3p config defined in ai_mode_button_config.json is
  // valid.
  for (const auto* config : ai_mode_button_config::kAiModeButtonConfigs) {
    SCOPED_TRACE(
        base::StringPrintf("Testing ID %d", static_cast<int>(config->id)));
    EXPECT_TRUE(TestAiModeButtonService::IsValidConfig(*config));
  }
}

TEST(AiModeButtonConfigTest, CompiledThirdPartyConfigsContainNoDuplicateIds) {
  // Verify that every single 3p config defined in ai_mode_button_config.json
  // has a unique `id`.
  std::set<SearchEngineType> seen;
  for (const auto* config : ai_mode_button_config::kAiModeButtonConfigs) {
    SCOPED_TRACE(
        base::StringPrintf("Testing ID %d", static_cast<int>(config->id)));
    EXPECT_TRUE(seen.insert(config->id).second);
  }
}

TEST_F(AiModeButtonServiceTest, DebugConfig) {
  // For manual testing, ai_mode_button_config.json contains a debug config
  // mapped to bing. Debug config shouldn't be returned when feature is
  // disabled.
  template_url_service()->SetUserSelectedDefaultSearchProvider(
      FindTurl(u"bing"));
  EXPECT_FALSE(service_->GetCurrentConfig());

  // Debug config shouldn't be returned when feature is enabled without the
  // debug param.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(omnibox::kAim3pEntrypoint);

    // Toggle DSE to force update.
    template_url_service()->SetUserSelectedDefaultSearchProvider(
        FindTurl(u"nongoogle"));
    EXPECT_FALSE(service_->GetCurrentConfig());
    template_url_service()->SetUserSelectedDefaultSearchProvider(
        FindTurl(u"bing"));
    EXPECT_FALSE(service_->GetCurrentConfig());
  }

  // Debug config should be returned when feature is enabled with the debug
  // param.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kAim3pEntrypoint, {{"Aim3pEntrypointDebug", "true"}});

    // Toggle DSE to force update.
    template_url_service()->SetUserSelectedDefaultSearchProvider(
        FindTurl(u"nongoogle"));
    EXPECT_FALSE(service_->GetCurrentConfig());
    template_url_service()->SetUserSelectedDefaultSearchProvider(
        FindTurl(u"bing"));
    const auto* config = service_->GetCurrentConfig();
    ASSERT_TRUE(config);
    EXPECT_EQ(config->id, SearchEngineType::SEARCH_ENGINE_BING);
    EXPECT_EQ(std::u16string_view(config->text), u"Google DEBÜG");
  }
}

}  // namespace
