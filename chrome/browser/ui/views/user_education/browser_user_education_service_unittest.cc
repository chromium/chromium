// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"

#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_variants_reader.h"
#include "base/threading/thread_restrictions.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/tutorial_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(BrowserUserEducationServiceTest, CheckFeaturePromoHistograms) {
  std::optional<base::HistogramVariantsEntryMap> iph_features;
  std::vector<std::string> missing_features;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    iph_features =
        base::ReadVariantsFromHistogramsXml("IPHFeature", "feature_engagement");
    ASSERT_TRUE(iph_features.has_value());
  }

  user_education::FeaturePromoRegistry registry;
  MaybeRegisterChromeFeaturePromos(registry);
  const auto& iph_specifications = registry.feature_data();
  for (const auto& [feature, spec] : iph_specifications) {
    if (!base::Contains(*iph_features, feature->name)) {
      missing_features.emplace_back(feature->name);
    }
  }
  ASSERT_TRUE(missing_features.empty())
      << "IPH Features:\n"
      << base::JoinString(missing_features, ", ")
      << "\nconfigured in browser_user_education_service.cc but no "
         "corresponding variants were added to IPHFeature variants in "
         "//tools/metrics/histograms/metadata/feature_engagement/"
         "histograms.xml";
}

TEST(BrowserUserEducationServiceTest, CheckNewBadgeHistograms) {
  std::optional<base::HistogramVariantsEntryMap> new_badge_features;
  std::vector<std::string> missing_features;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    new_badge_features = base::ReadVariantsFromHistogramsXml("NewBadgeFeature",
                                                             "user_education");
    ASSERT_TRUE(new_badge_features.has_value());
  }
  user_education::NewBadgeRegistry registry;
  MaybeRegisterChromeNewBadges(registry);
  const auto& new_badge_specifications = registry.feature_data();
  for (const auto& [feature, spec] : new_badge_specifications) {
    if (!base::Contains(*new_badge_features, feature->name)) {
      missing_features.emplace_back(feature->name);
    }
  }
  ASSERT_TRUE(missing_features.empty())
      << "New Badge Features:\n"
      << base::JoinString(missing_features, ", ")
      << "\nconfigured in browser_user_education_service.cc but no "
         "corresponding variants were added to NewBadgeFeature variants in "
         "//tools/metrics/histograms/metadata/user_education/histograms.xml";
}

TEST(BrowserUserEducationServiceTest, CheckTutorialHistograms) {
  std::optional<base::HistogramVariantsEntryMap> tutorial_features;
  std::vector<std::string> missing_features;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    tutorial_features =
        base::ReadVariantsFromHistogramsXml("TutorialID", "user_education");
    ASSERT_TRUE(tutorial_features.has_value());
  }
  user_education::TutorialRegistry registry;
  MaybeRegisterChromeTutorials(registry);
  const auto& tutorial_identifiers = registry.GetTutorialIdentifiers();
  for (const auto& identifier : tutorial_identifiers) {
    const auto* tutorial = registry.GetTutorialDescription(identifier);
    ASSERT_NE(nullptr, tutorial->histograms)
        << "Tutorials must be created with a histogram prefix";
    auto variant_name =
        std::string(".").append(tutorial->histograms->GetTutorialPrefix());
    if (!base::Contains(*tutorial_features, variant_name)) {
      missing_features.emplace_back(variant_name);
    }
  }
  ASSERT_TRUE(missing_features.empty())
      << "Tutorial Features:\n"
      << base::JoinString(missing_features, ", ")
      << "\nconfigured in browser_user_education_service.cc but no "
         "corresponding variants were added to TutorialID variants in "
         "//tools/metrics/histograms/metadata/user_education/histograms.xml";
}
