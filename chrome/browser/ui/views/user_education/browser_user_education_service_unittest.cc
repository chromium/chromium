// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"

#include <iterator>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/test/metrics/action_suffix_reader.h"
#include "base/test/metrics/histogram_variants_reader.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_configurations.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/tutorial_registry.h"
#include "components/user_education/common/user_education_metadata.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Checks metadata and returns a list of errors.
std::vector<std::string> CheckMetadata(
    const user_education::Metadata& metadata) {
  std::vector<std::string> errors;
  if (!metadata.launch_milestone) {
    errors.emplace_back("launch milestone");
  }
  if (metadata.owners.empty()) {
    errors.emplace_back("owners list");
  }
  if (metadata.additional_description.empty()) {
    errors.emplace_back("description");
  }
  return errors;
}

template <size_t N>
bool Contains(const char* const (&values)[N], const std::string& value) {
  return std::find(std::begin(values), std::end(values), value) !=
         std::end(values);
}

}  // namespace

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

TEST(BrowserUserEducationServiceTest, CheckFeaturePromoActions) {
  std::vector<base::ActionSuffixEntryMap> iph_suffixes;
  std::vector<std::string> missing_features;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    iph_suffixes =
        base::ReadActionSuffixesForAction("UserEducation.MessageShown.IPH");
    ASSERT_EQ(1U, iph_suffixes.size());
  }

  user_education::FeaturePromoRegistry registry;
  MaybeRegisterChromeFeaturePromos(registry);
  const auto& iph_specifications = registry.feature_data();
  for (const auto& [feature, spec] : iph_specifications) {
    std::string feature_name = feature->name;
    if (feature_name.starts_with("IPH_")) {
      feature_name = feature_name.substr(4);
    }
    if (!base::Contains(iph_suffixes[0], feature_name)) {
      missing_features.emplace_back(feature->name);
    }
  }
  ASSERT_TRUE(missing_features.empty())
      << "IPH Features:\n"
      << base::JoinString(missing_features, ", ")
      << "\nconfigured in browser_user_education_service.cc but no "
         "corresponding action suffixes were added in "
         "//tools/metrics/actions/actions.xml";
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
  std::map<std::string, std::string> known_histograms;
  std::vector<std::tuple<std::string, std::string, std::string>>
      histogram_collisions;
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
    const auto variant_name = tutorial->histograms->GetTutorialPrefix();
    if (!known_histograms.emplace(variant_name, identifier).second) {
      histogram_collisions.emplace_back(
          identifier, known_histograms[identifier], variant_name);
    }
    if (!base::Contains(*tutorial_features, variant_name)) {
      missing_features.emplace_back(variant_name);
    }
  }
  EXPECT_TRUE(missing_features.empty())
      << "Tutorial Features:\n"
      << base::JoinString(missing_features, ", ")
      << "\nconfigured in browser_user_education_service.cc but no "
         "corresponding variants were added to TutorialID variants in "
         "//tools/metrics/histograms/metadata/user_education/histograms.xml";
  for (const auto& [t1, t2, histogram] : histogram_collisions) {
    ADD_FAILURE() << "Error: tutorial " << t1 << " and " << t2
                  << " share histogram " << histogram;
  }
  EXPECT_TRUE(histogram_collisions.empty());
}

TEST(BrowserUserEducationServiceTest, PreventNewHardCodedConfigurations) {
  const base::Feature* const kAllowedConfigurations[] = {
      // To be triaged:
      //
      // (These are listed because they were present prior to this test being
      // written; in the future as many as possible should be eliminated, and
      // the rest moved down to the "explicitly allowed" list below.)
      //
      // DO NOT ADD ENTRIES TO THIS LIST, EVER.
      &feature_engagement::kIPHBatterySaverModeFeature,
      &feature_engagement::kIPHCompanionSidePanelFeature,
      &feature_engagement::kIPHComposeMSBBSettingsFeature,
      &feature_engagement::kIPHDesktopSharedHighlightingFeature,
      &feature_engagement::kIPHDiscardRingFeature,
      &feature_engagement::kIPHDownloadEsbPromoFeature,
      &feature_engagement::kIPHExperimentalAIPromoFeature,
      &feature_engagement::kIPHExtensionsMenuFeature,
      &feature_engagement::kIPHExtensionsRequestAccessButtonFeature,
      &feature_engagement::kIPHGMCCastStartStopFeature,
      &feature_engagement::kIPHGMCLocalMediaCastingFeature,
      &feature_engagement::kIPHMemorySaverModeFeature,
      &feature_engagement::kIPHPasswordsManagementBubbleAfterSaveFeature,
      &feature_engagement::kIPHPasswordsManagementBubbleDuringSigninFeature,
      &feature_engagement::kIPHPasswordsWebAppProfileSwitchFeature,
      &feature_engagement::kIPHPasswordManagerShortcutFeature,
      &feature_engagement::kIPHPasswordSharingFeature,
      &feature_engagement::kIPHPowerBookmarksSidePanelFeature,
      &feature_engagement::kIPHReadingListInSidePanelFeature,
      &feature_engagement::kIPHReadingModeSidePanelFeature,
      &feature_engagement::kIPHSidePanelGenericPinnableFeature,
      &feature_engagement::kIPHSignoutWebInterceptFeature,
      &feature_engagement::kIPHTabOrganizationSuccessFeature,
      &feature_engagement::kIPHProfileSwitchFeature,
      &feature_engagement::kIPHPriceTrackingInSidePanelFeature,
      &feature_engagement::kIPHBackNavigationMenuFeature,
      &feature_engagement::kIPHAutofillCreditCardBenefitFeature,
      &feature_engagement::kIPHAutofillExternalAccountProfileSuggestionFeature,
      &feature_engagement::kIPHAutofillManualFallbackFeature,
      &feature_engagement::kIPHAutofillVirtualCardCVCSuggestionFeature,
      &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature,
      &feature_engagement::kIPHCookieControlsFeature,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      &feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch
#endif
      // Explicitly allowed:
      //
      // (These have been cleared by Frizzle Team as requiring their own
      // specific configuration.)
      //
      // DO NOT ADD ENTRIES TO THIS LIST WITHOUT APPROVAL FROM
      // components/user_education/OWNERS
  };

  std::vector<std::string> invalid_configs;

  user_education::FeaturePromoRegistry registry;
  MaybeRegisterChromeFeaturePromos(registry);
  const auto& iph_specifications = registry.feature_data();
  for (const auto& [feature, spec] : iph_specifications) {
    const auto config = feature_engagement::GetClientSideFeatureConfig(feature);
    if (config && !Contains(kAllowedConfigurations, feature)) {
      invalid_configs.emplace_back(feature->name);
    }
  }

  ASSERT_TRUE(invalid_configs.empty())
      << "Disallowed desktop Chrome entries in feature_configurations.cc:\n"
      << base::JoinString(invalid_configs, "\n")
      << "\nPlease note that configurations are automatically generated for"
         " Desktop IPH. In nearly all cases, this auto-configuration (plus"
         " additional configuration options in FeaturePromoSpecification)"
         " should be sufficient, and adding to feature_configurations.cc"
         " should be avoided.";
}

TEST(BrowserUserEducationServiceTest, CheckFeaturePromoMetadata) {
  // These promos get a pass because they are old and never had metadata
  // associated with them. All new promos should have metadata.
  //
  // DO NOT ADD ENTRIES TO THIS LIST, EVER.
  const base::Feature* const kExistingPromosWithoutMetadata[] = {
      &feature_engagement::kIPHComposeMSBBSettingsFeature,
      &feature_engagement::kIPHDesktopSharedHighlightingFeature,
      &feature_engagement::kIPHDesktopCustomizeChromeFeature,
      &feature_engagement::kIPHExperimentalAIPromoFeature,
      &feature_engagement::kIPHExplicitBrowserSigninPreferenceRememberedFeature,
      &feature_engagement::kIPHGMCCastStartStopFeature,
      &feature_engagement::kIPHGMCLocalMediaCastingFeature,
      &feature_engagement::kIPHLiveCaptionFeature,
      &feature_engagement::kIPHTabAudioMutingFeature,
      &feature_engagement::kIPHPasswordsManagementBubbleDuringSigninFeature,
      &feature_engagement::kIPHPasswordsWebAppProfileSwitchFeature,
      &feature_engagement::kIPHPasswordManagerShortcutFeature,
      &feature_engagement::kIPHPasswordSharingFeature,
      &feature_engagement::kIPHReadingListDiscoveryFeature,
      &feature_engagement::kIPHReadingListEntryPointFeature,
      &feature_engagement::kIPHReadingListInSidePanelFeature,
      &feature_engagement::kIPHSignoutWebInterceptFeature,
      &feature_engagement::kIPHProfileSwitchFeature,
      &feature_engagement::kIPHBackNavigationMenuFeature,
      &feature_engagement::kIPHCookieControlsFeature};

  user_education::FeaturePromoRegistry registry;
  MaybeRegisterChromeFeaturePromos(registry);
  const auto& iph_specifications = registry.feature_data();
  std::ostringstream oss;
  bool failed = false;
  for (const auto& [feature, spec] : iph_specifications) {
    const auto errors = CheckMetadata(spec.metadata());
    if (!errors.empty() && !Contains(kExistingPromosWithoutMetadata, feature)) {
      failed = true;
      oss << "\n"
          << feature->name
          << " is missing metadata: " << base::JoinString(errors, ", ");
    }
  }
  EXPECT_FALSE(failed) << "Feature promos missing metadata:" << oss.str();
}

TEST(BrowserUserEducationServiceTest, CheckTutorialPromoMetadata) {
  user_education::TutorialRegistry registry;
  MaybeRegisterChromeTutorials(registry);
  const auto& tutorial_identifiers = registry.GetTutorialIdentifiers();
  std::ostringstream oss;
  bool failed = false;
  for (const auto& identifier : tutorial_identifiers) {
    const auto* tutorial = registry.GetTutorialDescription(identifier);
    const auto errors = CheckMetadata(tutorial->metadata);
    if (!errors.empty()) {
      failed = true;
      oss << "\n"
          << identifier
          << " is missing metadata: " << base::JoinString(errors, ", ");
    }
  }
  EXPECT_FALSE(failed) << "Tutorials missing metadata:" << oss.str();
}

TEST(BrowserUserEducationServiceTest, CheckNewBadgeMetadata) {
  user_education::NewBadgeRegistry registry;
  MaybeRegisterChromeNewBadges(registry);
  const auto& new_badge_specifications = registry.feature_data();
  std::ostringstream oss;
  bool failed = false;
  for (const auto& [feature, spec] : new_badge_specifications) {
    const auto errors = CheckMetadata(spec.metadata);
    if (!errors.empty()) {
      failed = true;
      oss << "\n"
          << feature->name
          << " is missing metadata: " << base::JoinString(errors, ", ");
    }
  }
  EXPECT_FALSE(failed) << "\"New\" Badges missing metadata:" << oss.str();
}
