// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_data.h"

#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/search_engines/regulatory_extension_type.h"
#include "components/search_engines/search_engines_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

namespace {
TemplateURLData BuildTestTemplateURLData(
    int id,
    base::span<TemplateURLData::RegulatoryExtension> extensions) {
  return TemplateURLData(
      /*name=*/u"shortname", /*keyword=*/u"keyword",
      /*search_url=*/"https://cs.chromium.org",
      /*suggest_url=*/{}, /*image_url=*/{}, /*image_translate_url=*/{},
      /*new_tab_url=*/{}, /*contextual_search_url=*/{}, /*logo_url=*/{},
      /*doodle_url=*/{}, /*search_url_post_params=*/{},
      /*suggest_url_post_params=*/{}, /*image_url_post_params=*/{},
      /*image_translate_source_language_param_key=*/{},
      /*image_translate_target_language_param_key=*/{},
      /*search_intent_params=*/{}, /*favicon_url=*/{}, /*encoding=*/{},
      /*image_search_branding_label=*/{}, /*alternate_urls_list=*/{},
      /*preconnect_to_search_url=*/false, /*prefetch_likely_navigations=*/false,
      /*id=*/id, /*regulatory_extensions=*/extensions);
}
}  // namespace

TEST(TemplateURLDataTest, Trim) {
  TemplateURLData data(BuildTestTemplateURLData(0, {}));

  EXPECT_EQ(u"shortname", data.short_name());
  EXPECT_EQ(u"keyword", data.keyword());

  data.SetShortName(u" othershortname ");
  data.SetKeyword(u" otherkeyword ");

  EXPECT_EQ(u"othershortname", data.short_name());
  EXPECT_EQ(u"otherkeyword", data.keyword());

  data.SetKeyword(u" other other keyword ");

  EXPECT_EQ(u"otherotherkeyword", data.keyword());
}

TEST(TemplateURLDataTest, AcceptKnownRegulatoryKeywords) {
  std::array<TemplateURLData::RegulatoryExtension, 2> extensions = {{
      {.variant = RegulatoryExtensionType::kDefault,
       .search_params = "default_search",
       .suggest_params = "default_suggest"},
      {.variant = RegulatoryExtensionType::kAndroidEEA,
       .search_params = "android_eea_search",
       .suggest_params = "android_eea_suggest"},
  }};

  auto data = BuildTestTemplateURLData(0, extensions);
  EXPECT_EQ("default_search",
            data.regulatory_extensions.at(RegulatoryExtensionType::kDefault)
                ->search_params);
  EXPECT_EQ("default_suggest",
            data.regulatory_extensions.at(RegulatoryExtensionType::kDefault)
                ->suggest_params);
  EXPECT_EQ("android_eea_search",
            data.regulatory_extensions.at(RegulatoryExtensionType::kAndroidEEA)
                ->search_params);
  EXPECT_EQ("android_eea_suggest",
            data.regulatory_extensions.at(RegulatoryExtensionType::kAndroidEEA)
                ->suggest_params);
}

#if DCHECK_IS_ON()
TEST(TemplateURLDataTest, DuplicateRegulatoryKeywords) {
  std::array<TemplateURLData::RegulatoryExtension, 2> duplicate_data = {{
      {RegulatoryExtensionType::kDefault, "default_data"},
      {RegulatoryExtensionType::kDefault, "android_eea_data"},
  }};

  EXPECT_DEATH_IF_SUPPORTED(BuildTestTemplateURLData(0, duplicate_data), "");
}
#endif

TEST(TemplateURLDataTest, PrepopulatedActiveStatus) {
  // Newly created TemplateURLData should be kUnspecified by default.
  TemplateURLData data1;
  ASSERT_EQ(data1.prepopulate_id, 0);
  EXPECT_EQ(data1.is_active, TemplateURLData::ActiveStatus::kUnspecified);

  TemplateURLData data2(BuildTestTemplateURLData(0, {}));
  ASSERT_EQ(data2.prepopulate_id, 0);
  EXPECT_EQ(data2.is_active, TemplateURLData::ActiveStatus::kUnspecified);

  // Prepopulated engines also have active status as kUnspecified by default.
  TemplateURLData data3(BuildTestTemplateURLData(1, {}));
  ASSERT_EQ(data3.prepopulate_id, 1);
  EXPECT_EQ(data3.is_active, TemplateURLData::ActiveStatus::kUnspecified);
}
