// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {
namespace {

class TestCountryNames : public CountryNames {
 public:
  explicit TestCountryNames(const std::string& locale_name)
      : CountryNames(locale_name) {}

  ~TestCountryNames() = default;
};

}  // namespace

// Test mapping of localized country names to country codes.
TEST(CountryNamesTest, GetCountryCode_BasicMapping) {
  TestCountryNames en_us_names("en_US");
  EXPECT_EQ("US", en_us_names.GetCountryCode(u"United States"));
  EXPECT_EQ("CA", en_us_names.GetCountryCode(u"Canada"));
  EXPECT_EQ("CZ", en_us_names.GetCountryCode(u"Czech Republic"));
}

TEST(CountryNamesTest, GetCountryCode_CaseInsensitiveMapping) {
  EXPECT_EQ("US", TestCountryNames("en_US").GetCountryCode(u"united states"));
}

TEST(CountryNamesTest, GetCountryCode_CodesMapToThemselves) {
  TestCountryNames en_us_names("en_US");
  TestCountryNames fr_ca_names("fr_CA");
  EXPECT_EQ("US", en_us_names.GetCountryCode(u"US"));
  EXPECT_EQ("HU", en_us_names.GetCountryCode(u"hu"));
  EXPECT_EQ("CA", fr_ca_names.GetCountryCode(u"CA"));
  EXPECT_EQ("MX", fr_ca_names.GetCountryCode(u"mx"));
}

TEST(CountryNamesTest, GetCountryCode_BasicSynonyms) {
  TestCountryNames en_us_names("en_US");
  EXPECT_EQ("US", en_us_names.GetCountryCode(u"United States of America"));
  EXPECT_EQ("US", en_us_names.GetCountryCode(u"USA"));
}

TEST(CountryNamesTest, GetCountryCode_OtherLocales) {
  EXPECT_EQ("US", TestCountryNames("es").GetCountryCode(u"Estados Unidos"));
  EXPECT_EQ("IT", TestCountryNames("it").GetCountryCode(u"Italia"));
  EXPECT_EQ("DE", TestCountryNames("nl").GetCountryCode(u"duitsland"));
}

TEST(CountryNamesTest, GetCountryCode_EnUsFallback) {
  TestCountryNames es_names("es");
  EXPECT_EQ("US", es_names.GetCountryCode(u"United States"));
  EXPECT_EQ("US", es_names.GetCountryCode(u"united states"));
  EXPECT_EQ("US", es_names.GetCountryCode(u"USA"));
}

TEST(CountryNamesTest, GetCountryCodeForLocalizedCountryName) {
  // Initialize with the default locale.
  TestCountryNames names("en_US");
  EXPECT_EQ("AM",
            names.GetCountryCodeForLocalizedCountryName(u"Armenien", "de"));
  // Check that there is no cache by requesting the same result twice.
  EXPECT_EQ("AM",
            names.GetCountryCodeForLocalizedCountryName(u"Armenien", "de"));
  EXPECT_EQ("AZ",
            names.GetCountryCodeForLocalizedCountryName(u"Azerbeidzjan", "nl"));
}

TEST(CountryNamesTest, GetCachedCountryCodeForLocalizedCountryName) {
  // Initialize with the default locale.
  TestCountryNames names("en_US");

  // Verify that the entry is not cached.
  EXPECT_FALSE(names.IsCountryNamesForLocaleCachedForTesting("de"));

  // Make a lookup of the entry that should result in a cache write.
  EXPECT_EQ("AM",
            names.GetCountryCodeForLocalizedCountryName(u"Armenien", "de"));

  // Verify that the entry is cached.
  EXPECT_TRUE(names.IsCountryNamesForLocaleCachedForTesting("de"));
}

// Test mapping of an empty country name to an country code.
TEST(CountryNamesTest, EmptyCountryNameHasEmptyCountryCode) {
  std::string country_code =
      TestCountryNames("en").GetCountryCode(std::u16string());
  EXPECT_TRUE(country_code.empty()) << country_code;
}

TEST(CountryNamesTest, GetCountryCodeInLocalLanguages) {
  base::span<const icu::Locale> available_locales = GetAvailableLocales();
  if (!base::Contains(available_locales, "de-IT")) {
    LOG(INFO) << "Skipping test because locale de-IT is not installed";
  }
  // Initialize with the default locale.
  TestCountryNames names("en_US");
  EXPECT_EQ("", names.GetCountryCode(u"Italien"));

  // Enable the new feature.
  // TODO(crbug.com/1135188): Delete this when the experiment is finished.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillCountryFromLocalName);

  // Verify that we still don't find the term Italien, because needs to know
  // at initialization time that the feature is enabled. Otherwise it does
  // not load the extra mappings to memory.
  EXPECT_EQ("", names.GetCountryCode(u"Italien"));

  // For a freshly initialized TestCountryNames instance, the local terms are
  // loaded.
  TestCountryNames names2("en_US");
  EXPECT_EQ("IT", names2.GetCountryCode(u"Italien"));
}

// Verify that the source of a classification is properly returned. This is
// just a temporary histogram can can be deleted later.
// TODO(crbug.com/1135188): Delete this when the experiment is finished.
TEST(CountryNamesTest, Sources) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillCountryFromLocalName);
  DetectionOfCountryName source;

  source = DetectionOfCountryName::kNotFound;
  EXPECT_EQ("US", TestCountryNames("en_US").GetCountryCode(u"U.S.A.", &source));
  EXPECT_EQ(source, DetectionOfCountryName::kCommonNames);

  source = DetectionOfCountryName::kNotFound;
  EXPECT_EQ("US",
            TestCountryNames("es").GetCountryCode(u"Estados Unidos", &source));
  EXPECT_EQ(source, DetectionOfCountryName::kApplicationLocale);

  source = DetectionOfCountryName::kNotFound;
  EXPECT_EQ("IT", TestCountryNames("es").GetCountryCode(u"Italien", &source));
  EXPECT_EQ(source, DetectionOfCountryName::kLocalLanguages);

  if (base::Contains(GetAvailableLocales(), "en-IT")) {
    LOG(INFO) << "Skipping test for en-IT";
  } else {
    source = DetectionOfCountryName::kNotFound;
    EXPECT_EQ("IT", TestCountryNames("es").GetCountryCode(u"Italy", &source));
    EXPECT_EQ(source, DetectionOfCountryName::kDefaultLocale);
  }

  // Assume app locale is "de" but website locale is French.
  source = DetectionOfCountryName::kNotFound;
  EXPECT_EQ("IT", TestCountryNames("de").GetCountryCodeForLocalizedCountryName(
                      u"Italie", "fr", &source));
  EXPECT_EQ(source, DetectionOfCountryName::kViaLanguageOfWebsite);

  source = DetectionOfCountryName::kNotFound;
  EXPECT_EQ("", TestCountryNames("en_US").GetCountryCode(u"Foo", &source));
  EXPECT_EQ(source, DetectionOfCountryName::kNotFound);
}

}  // namespace autofill
