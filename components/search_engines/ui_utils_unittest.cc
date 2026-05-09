// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/ui_utils.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

namespace {

// Helper function to create a TemplateURL with specified properties
std::unique_ptr<TemplateURL> CreateTemplateURL(const std::u16string& short_name,
                                               const std::u16string& keyword,
                                               const std::string& url,
                                               bool created_by_policy = false,
                                               int prepopulate_id = 0) {
  TemplateURLData data;
  data.SetShortName(short_name);
  data.SetKeyword(keyword);
  data.SetURL(url);
  data.is_active = TemplateURLData::ActiveStatus::kTrue;
  data.policy_origin = created_by_policy
                           ? TemplateURLData::PolicyOrigin::kSiteSearch
                           : TemplateURLData::PolicyOrigin::kNoPolicy;
  data.prepopulate_id = prepopulate_id;
  return std::make_unique<TemplateURL>(data);
}

}  // namespace

// Test fixture for the internal sorting logic
class OrderTemplateUrlsByManagedAndAlphabeticallyTest : public testing::Test {
 public:
  OrderTemplateUrlsByManagedAndAlphabeticallyTest() = default;
  ~OrderTemplateUrlsByManagedAndAlphabeticallyTest() override = default;

 protected:
  void SetUp() override {
    comparator_ = std::make_unique<
        internal::OrderTemplateUrlsByManagedAndAlphabetically>();
  }

  std::unique_ptr<internal::OrderTemplateUrlsByManagedAndAlphabetically>
      comparator_;
};

// Test basic alphabetical sorting
TEST_F(OrderTemplateUrlsByManagedAndAlphabeticallyTest,
       BasicAlphabeticalSorting) {
  auto google = CreateTemplateURL(u"Google", u"google.com",
                                  "https://google.com/search?q={searchTerms}");
  auto bing = CreateTemplateURL(u"Bing", u"bing.com",
                                "https://bing.com/search?q={searchTerms}");
  auto yahoo = CreateTemplateURL(u"Yahoo", u"yahoo.com",
                                 "https://yahoo.com/search?q={searchTerms}");

  // Bing should come before Google, Google before Yahoo
  EXPECT_TRUE((*comparator_)(bing.get(), google.get()));
  EXPECT_TRUE((*comparator_)(google.get(), yahoo.get()));
  EXPECT_TRUE((*comparator_)(bing.get(), yahoo.get()));

  // Reverse comparisons should be false
  EXPECT_FALSE((*comparator_)(google.get(), bing.get()));
  EXPECT_FALSE((*comparator_)(yahoo.get(), google.get()));
  EXPECT_FALSE((*comparator_)(yahoo.get(), bing.get()));
}

// Test that managed engines come before non-managed engines
TEST_F(OrderTemplateUrlsByManagedAndAlphabeticallyTest, ManagedEnginesFirst) {
  auto managed_yahoo = CreateTemplateURL(
      u"Yahoo", u"yahoo.com", "https://yahoo.com/search?q={searchTerms}", true);
  auto regular_bing = CreateTemplateURL(
      u"Bing", u"bing.com", "https://bing.com/search?q={searchTerms}", false);

  // Even though Bing comes before Yahoo alphabetically, managed Yahoo should
  // come first
  EXPECT_TRUE((*comparator_)(managed_yahoo.get(), regular_bing.get()));
  EXPECT_FALSE((*comparator_)(regular_bing.get(), managed_yahoo.get()));
}

// Test sorting within managed and non-managed groups
TEST_F(OrderTemplateUrlsByManagedAndAlphabeticallyTest, SortingWithinGroups) {
  auto managed_google =
      CreateTemplateURL(u"Google", u"google.com",
                        "https://google.com/search?q={searchTerms}", true);
  auto managed_bing = CreateTemplateURL(
      u"Bing", u"bing.com", "https://bing.com/search?q={searchTerms}", true);
  auto regular_yahoo =
      CreateTemplateURL(u"Yahoo", u"yahoo.com",
                        "https://yahoo.com/search?q={searchTerms}", false);
  auto regular_ask = CreateTemplateURL(
      u"Ask", u"ask.com", "https://ask.com/search?q={searchTerms}", false);

  // Within managed group: Bing before Google
  EXPECT_TRUE((*comparator_)(managed_bing.get(), managed_google.get()));

  // Within regular group: Ask before Yahoo
  EXPECT_TRUE((*comparator_)(regular_ask.get(), regular_yahoo.get()));

  // Managed engines before regular engines
  EXPECT_TRUE((*comparator_)(managed_google.get(), regular_ask.get()));
  EXPECT_TRUE((*comparator_)(managed_bing.get(), regular_yahoo.get()));
}

// Test case-insensitive sorting
TEST_F(OrderTemplateUrlsByManagedAndAlphabeticallyTest,
       CaseInsensitiveSorting) {
  auto lower_google = CreateTemplateURL(
      u"google", u"google.com", "https://google.com/search?q={searchTerms}");
  auto upper_bing = CreateTemplateURL(
      u"BING", u"bing.com", "https://bing.com/search?q={searchTerms}");

  // Should still sort alphabetically regardless of case
  EXPECT_TRUE((*comparator_)(upper_bing.get(), lower_google.get()));
  EXPECT_FALSE((*comparator_)(lower_google.get(), upper_bing.get()));
}

// Test Unicode string handling
TEST_F(OrderTemplateUrlsByManagedAndAlphabeticallyTest, UnicodeStringHandling) {
  auto ascii_engine = CreateTemplateURL(
      u"Google", u"google.com", "https://google.com/search?q={searchTerms}");
  auto unicode_engine = CreateTemplateURL(
      u"Яндекс", u"yandex.ru", "https://yandex.ru/search?q={searchTerms}");
  auto accented_engine = CreateTemplateURL(
      u"Écosía", u"ecosia.org", "https://ecosia.org/search?q={searchTerms}");

  // The exact sorting order may depend on locale, but the function should not
  // crash and should produce consistent results
  bool result1 = (*comparator_)(ascii_engine.get(), unicode_engine.get());
  bool result2 = (*comparator_)(unicode_engine.get(), ascii_engine.get());
  EXPECT_NE(result1, result2);  // One should be true, the other false

  bool result3 = (*comparator_)(ascii_engine.get(), accented_engine.get());
  bool result4 = (*comparator_)(accented_engine.get(), ascii_engine.get());
  EXPECT_NE(result3, result4);
}

// Test fallback to keyword when short names are identical
TEST_F(OrderTemplateUrlsByManagedAndAlphabeticallyTest, FallbackToKeyword) {
  auto engine1 = CreateTemplateURL(
      u"Search", u"search1.com", "https://search1.com/search?q={searchTerms}");
  auto engine2 = CreateTemplateURL(
      u"Search", u"search2.com", "https://search2.com/search?q={searchTerms}");

  // Should sort by keyword when short names are identical
  EXPECT_TRUE((*comparator_)(engine1.get(), engine2.get()));
  EXPECT_FALSE((*comparator_)(engine2.get(), engine1.get()));
}

// Test buffer overflow protection in GetShortNameSortKey
TEST_F(OrderTemplateUrlsByManagedAndAlphabeticallyTest,
       BufferOverflowProtection) {
  // Create a very long string that would exceed the 1000 byte buffer
  std::u16string very_long_name;
  // Use multi-byte Unicode characters to more easily exceed buffer
  for (int i = 0; i < 500; i++) {
    very_long_name += u"测试";  // Each character is 3 bytes in UTF-8
  }

  auto long_name_engine =
      CreateTemplateURL(very_long_name, u"longname.com",
                        "https://longname.com/search?q={searchTerms}");
  auto short_name_engine = CreateTemplateURL(
      u"Short", u"short.com", "https://short.com/search?q={searchTerms}");

  // Should not crash even with very long names
  bool result1 =
      (*comparator_)(long_name_engine.get(), short_name_engine.get());
  bool result2 =
      (*comparator_)(short_name_engine.get(), long_name_engine.get());

  // Results should be consistent (one true, one false)
  EXPECT_NE(result1, result2);

  // Test that GetShortNameSortKey doesn't crash with very long strings
  std::string sort_key =
      comparator_->GetShortNameSortKeyForTesting(very_long_name);
  EXPECT_FALSE(sort_key.empty());

  // The sort key should be truncated but still valid
  EXPECT_LT(sort_key.length(), 1000u);
}

// Test GetShortNameSortKey with various Unicode scenarios
TEST_F(OrderTemplateUrlsByManagedAndAlphabeticallyTest,
       GetShortNameSortKeyUnicode) {
  // Test with ASCII
  std::string ascii_key = comparator_->GetShortNameSortKeyForTesting(u"Google");
  EXPECT_FALSE(ascii_key.empty());

  // Test with Unicode
  std::string unicode_key =
      comparator_->GetShortNameSortKeyForTesting(u"Яндекс");
  EXPECT_FALSE(unicode_key.empty());

  // Test with accented characters
  std::string accented_key =
      comparator_->GetShortNameSortKeyForTesting(u"Écosía");
  EXPECT_FALSE(accented_key.empty());

  // Test with empty string
  std::string empty_key = comparator_->GetShortNameSortKeyForTesting(u"");
  // Empty string should produce empty or very short key
  EXPECT_LE(empty_key.length(), 1u);
}

// Test edge cases with special characters
TEST_F(OrderTemplateUrlsByManagedAndAlphabeticallyTest, SpecialCharacters) {
  auto numeric_engine =
      CreateTemplateURL(u"123Search", u"123search.com",
                        "https://123search.com/search?q={searchTerms}");
  auto symbol_engine =
      CreateTemplateURL(u"@Search", u"atsearch.com",
                        "https://atsearch.com/search?q={searchTerms}");
  auto mixed_engine =
      CreateTemplateURL(u"A1@Search", u"a1search.com",
                        "https://a1search.com/search?q={searchTerms}");

  // Should handle special characters without crashing
  bool result1 = (*comparator_)(numeric_engine.get(), symbol_engine.get());
  bool result2 = (*comparator_)(symbol_engine.get(), mixed_engine.get());
  bool result3 = (*comparator_)(numeric_engine.get(), mixed_engine.get());

  // Results should be consistent
  EXPECT_EQ(result1,
            !(*comparator_)(symbol_engine.get(), numeric_engine.get()));
  EXPECT_EQ(result2, !(*comparator_)(mixed_engine.get(), symbol_engine.get()));
  EXPECT_EQ(result3, !(*comparator_)(mixed_engine.get(), numeric_engine.get()));
}

// Test with extremely long Unicode sequences that could cause buffer issues
TEST_F(OrderTemplateUrlsByManagedAndAlphabeticallyTest, ExtremelyLongUnicode) {
  // Create string with combining characters that could expand during
  // normalization
  std::u16string base_char = u"a";
  std::u16string combining_chars;
  for (int i = 0; i < 200; i++) {
    combining_chars += u"\u0301";  // Combining acute accent
  }
  std::u16string extreme_string = base_char + combining_chars;

  auto extreme_engine =
      CreateTemplateURL(extreme_string, u"extreme.com",
                        "https://extreme.com/search?q={searchTerms}");
  auto normal_engine = CreateTemplateURL(
      u"Normal", u"normal.com", "https://normal.com/search?q={searchTerms}");

  // Should handle extreme Unicode without crashing
  EXPECT_NO_FATAL_FAILURE({
    bool result = (*comparator_)(extreme_engine.get(), normal_engine.get());
    (void)result;  // Suppress unused variable warning
  });

  // GetShortNameSortKey should handle this without crashing
  EXPECT_NO_FATAL_FAILURE({
    std::string sort_key =
        comparator_->GetShortNameSortKeyForTesting(extreme_string);
    EXPECT_LE(sort_key.length(), 1000u);
  });
}

// Test with exact buffer boundary conditions
TEST_F(OrderTemplateUrlsByManagedAndAlphabeticallyTest,
       BufferBoundaryConditions) {
  // Test string that should be exactly at buffer limit
  std::u16string near_limit_string;
  // Create a string that when converted to sort key approaches the 1000 byte
  // limit
  for (int i = 0; i < 100; i++) {
    near_limit_string += u"abcdefghij";  // 10 ASCII chars per iteration
  }

  auto near_limit_engine =
      CreateTemplateURL(near_limit_string, u"nearlimit.com",
                        "https://nearlimit.com/search?q={searchTerms}");
  auto normal_engine = CreateTemplateURL(
      u"Normal", u"normal.com", "https://normal.com/search?q={searchTerms}");

  // Should handle near-limit strings without issues
  EXPECT_NO_FATAL_FAILURE({
    bool result = (*comparator_)(near_limit_engine.get(), normal_engine.get());
    std::string sort_key =
        comparator_->GetShortNameSortKeyForTesting(near_limit_string);
    EXPECT_FALSE(sort_key.empty());
    EXPECT_LT(sort_key.length(), 1000u);
    (void)result;  // Suppress unused variable warning
  });
}

class OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically
    : public testing::Test {};

// Test that regional prepopulated engines are added first, then non-regional
// ones, then custom ones.
TEST_F(OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically,
       PrepopulatedEnginesFirst) {
  std::unique_ptr<TemplateURLData> bing_data =
      TemplateURLDataFromPrepopulatedEngine(TemplateURLPrepopulateData::bing);

  auto bing_engine = CreateTemplateURL(
      u"Bing", u"bing.com", "https://bing.com/search?q={searchTerms}",
      /*created_by_policy=*/false, bing_data->prepopulate_id);
  auto prepopulated_engine = CreateTemplateURL(
      u"CustomPrepop", u"customprepop.com",
      "https://customprepop.com/search?q={searchTerms}",
      /*created_by_policy=*/false, bing_data->prepopulate_id + 1);
  auto custom_engine =
      CreateTemplateURL(u"A-Engine", u"aengine.com",
                        "https://aengine.com/search?q={searchTerms}");

  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls;
  prepopulated_urls.push_back(std::move(bing_data));

  auto comparator = std::make_unique<
      internal::OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically>(
      std::move(prepopulated_urls));

  // Even though the alphabetical order is different, the prepopulated engines
  // are added first.
  EXPECT_TRUE((*comparator)(bing_engine.get(), prepopulated_engine.get()));
  EXPECT_FALSE((*comparator)(prepopulated_engine.get(), bing_engine.get()));

  EXPECT_TRUE((*comparator)(bing_engine.get(), custom_engine.get()));
  EXPECT_FALSE((*comparator)(custom_engine.get(), bing_engine.get()));

  EXPECT_TRUE((*comparator)(prepopulated_engine.get(), custom_engine.get()));
  EXPECT_FALSE((*comparator)(custom_engine.get(), prepopulated_engine.get()));
}

// Test that prepopulated engines are added in the order of
// `prepopulated_engines`.
TEST_F(OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically,
       PrepopulatedEnginesSorted) {
  std::unique_ptr<TemplateURLData> yahoo_data =
      TemplateURLDataFromPrepopulatedEngine(TemplateURLPrepopulateData::yahoo);
  std::unique_ptr<TemplateURLData> bing_data =
      TemplateURLDataFromPrepopulatedEngine(TemplateURLPrepopulateData::bing);
  std::unique_ptr<TemplateURLData> google_data =
      TemplateURLDataFromPrepopulatedEngine(TemplateURLPrepopulateData::google);

  auto yahoo_engine = CreateTemplateURL(
      u"Yahoo", u"yahoo.com", "https://yahoo.com/search?q={searchTerms}",
      /*created_by_policy=*/false, yahoo_data->prepopulate_id);
  auto bing_engine = CreateTemplateURL(
      u"Bing", u"bing.com", "https://bing.com/search?q={searchTerms}",
      /*created_by_policy=*/false, bing_data->prepopulate_id);
  auto google_engine = CreateTemplateURL(
      u"Google", u"google.com", "https://google.com/search?q={searchTerms}",
      /*created_by_policy=*/false, google_data->prepopulate_id);

  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls;
  prepopulated_urls.push_back(std::move(yahoo_data));
  prepopulated_urls.push_back(std::move(bing_data));
  prepopulated_urls.push_back(std::move(google_data));

  auto comparator = std::make_unique<
      internal::OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically>(
      std::move(prepopulated_urls));

  // Even though the alphabetical order is different, the engines are added in
  // the order of `prepopulated_urls`.
  EXPECT_TRUE((*comparator)(yahoo_engine.get(), bing_engine.get()));
  EXPECT_FALSE((*comparator)(bing_engine.get(), yahoo_engine.get()));

  EXPECT_TRUE((*comparator)(yahoo_engine.get(), google_engine.get()));
  EXPECT_FALSE((*comparator)(google_engine.get(), yahoo_engine.get()));

  EXPECT_TRUE((*comparator)(bing_engine.get(), google_engine.get()));
  EXPECT_FALSE((*comparator)(google_engine.get(), bing_engine.get()));
}

class GetDisabledStarterPackIdsTest : public testing::Test {};

TEST_F(GetDisabledStarterPackIdsTest, AiMode) {
  auto disabled_ids_with_ai = internal::GetDisabledStarterPackIds(
      /*ai_mode_enabled=*/true, /*gemini_enabled=*/true);
  EXPECT_FALSE(disabled_ids_with_ai.Has(
      template_url_starter_pack_data::StarterPackId::kAiMode));

  auto disabled_ids_without_ai = internal::GetDisabledStarterPackIds(
      /*ai_mode_enabled=*/false, /*gemini_enabled=*/true);
  EXPECT_TRUE(disabled_ids_without_ai.Has(
      template_url_starter_pack_data::StarterPackId::kAiMode));
}

TEST_F(GetDisabledStarterPackIdsTest, GeminiWithStarterPackExpansion) {
  base::test::ScopedFeatureList feature_list{omnibox::kStarterPackExpansion};

  auto disabled_ids = internal::GetDisabledStarterPackIds(
      /*ai_mode_enabled=*/true, /*gemini_enabled=*/true);
  EXPECT_FALSE(
      disabled_ids.Has(template_url_starter_pack_data::StarterPackId::kGemini));
}

TEST_F(GetDisabledStarterPackIdsTest, GeminiWithoutStarterPackExpansion) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::kStarterPackExpansion);

  auto disabled_ids = internal::GetDisabledStarterPackIds(
      /*ai_mode_enabled=*/true, /*gemini_enabled=*/false);
  EXPECT_TRUE(
      disabled_ids.Has(template_url_starter_pack_data::StarterPackId::kGemini));
}

TEST_F(GetDisabledStarterPackIdsTest, PageWithStarterPackPage) {
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::ContextualSearch>
      scoped_config;
  scoped_config.Get().starter_pack_page = true;

  auto disabled_ids = internal::GetDisabledStarterPackIds(
      /*ai_mode_enabled=*/true, /*gemini_enabled=*/true);
  EXPECT_FALSE(
      disabled_ids.Has(template_url_starter_pack_data::StarterPackId::kPage));
}

TEST_F(GetDisabledStarterPackIdsTest, PageWithoutStarterPackPage) {
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::ContextualSearch>
      scoped_config;
  scoped_config.Get().starter_pack_page = false;

  auto disabled_ids = internal::GetDisabledStarterPackIds(
      /*ai_mode_enabled=*/true, /*gemini_enabled=*/true);
  EXPECT_TRUE(
      disabled_ids.Has(template_url_starter_pack_data::StarterPackId::kPage));
}
