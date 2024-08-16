// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_data.h"

#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/search_engines/prepopulated_engines.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
TemplateURLData BuildDataForRegulatoryExtensions(
    base::span<TemplateURLData::RegulatoryExtension> extensions) {
  return TemplateURLData(
      u"shortname", u"keyword", "https://cs.chromium.org", std::string_view(),
      std::string_view(), std::string_view(), std::string_view(),
      std::string_view(), std::string_view(), std::string_view(),
      std::string_view(), std::string_view(), std::string_view(),
      std::string_view(), std::string_view(), std::string_view(),
      std::string_view(), {}, std::string_view(), std::string_view(),
      std::u16string_view(), base::Value::List(), false, false, 0, extensions);
}
}  // namespace

TEST(TemplateURLDataTest, Trim) {
  TemplateURLData data(
      u" shortname ", u" keyword ", "https://cs.chromium.org",
      std::string_view(), std::string_view(), std::string_view(),
      std::string_view(), std::string_view(), std::string_view(),
      std::string_view(), std::string_view(), std::string_view(),
      std::string_view(), std::string_view(), std::string_view(),
      std::string_view(), std::string_view(), {}, std::string_view(),
      std::string_view(), std::u16string_view(), base::Value::List(), false,
      false, 0, {});

  EXPECT_EQ(u"shortname", data.short_name());
  EXPECT_EQ(u"keyword", data.keyword());

  data.SetShortName(u" othershortname ");
  data.SetKeyword(u" otherkeyword ");

  EXPECT_EQ(u"othershortname", data.short_name());
  EXPECT_EQ(u"otherkeyword", data.keyword());
}

#if DCHECK_IS_ON()
TEST(TemplateURLDataTest, RejectUnknownRegulatoryKeywords) {
  std::array<TemplateURLData::RegulatoryExtension, 2> unknown_keywords = {{
      {"default", "good data"},
      {"unknown", "bad data"},
  }};

  EXPECT_DEATH_IF_SUPPORTED(BuildDataForRegulatoryExtensions(unknown_keywords),
                            "");
}
#endif

TEST(TemplateURLDataTest, AcceptKnownRegulatoryKeywords) {
  std::array<TemplateURLData::RegulatoryExtension, 2> extensions = {{
      {"default", "default_data"},
      {"android_eea", "android_eea_data"},
  }};

  auto data = BuildDataForRegulatoryExtensions(extensions);
  EXPECT_EQ("default_data", data.regulatory_extensions.at("default")->params);
  EXPECT_EQ("android_eea_data",
            data.regulatory_extensions.at("android_eea")->params);
}

#if DCHECK_IS_ON()
TEST(TemplateURLDataTest, DuplicateRegulatoryKeywords) {
  std::array<TemplateURLData::RegulatoryExtension, 2> duplicate_data = {{
      {"default", "default_data"},
      {"default", "android_eea_data"},
  }};

  EXPECT_DEATH_IF_SUPPORTED(BuildDataForRegulatoryExtensions(duplicate_data),
                            "");
}
#endif
