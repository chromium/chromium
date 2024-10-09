// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"

#include <cstdint>
#include <map>
#include <optional>
#include <ostream>
#include <sstream>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_configurations.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/tutorial_identifier.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace {

enum class IPHFailureReason {
  kNone,
  kUnlisted,
  kWrongSessionRate,
  kWrongSessionImpact,
  kWrongSessionImpactToast,
  kWrongSessionImpactKeyedNotice,
  kWrongSessionImpactLegalNotice,
  kWrongSessionImpactActionableAlert,
  kLegacyPromoNoScreenReader,
  kWrongSessionParamsRotatingPromo,
};

struct IPHException {
  IPHException() = default;
  IPHException(const base::Feature* feature_,
               std::optional<IPHFailureReason> reason_,
               const char* description_)
      : feature(feature_), reason(reason_), description(description_) {}
  IPHException(const IPHException& other) = default;
  IPHException& operator=(const IPHException& other) = default;
  ~IPHException() = default;

  raw_ptr<const base::Feature> feature = nullptr;
  std::optional<IPHFailureReason> reason;
  const char* description = nullptr;
};

struct IPHFailure {
  IPHFailure() = default;
  IPHFailure(const base::Feature* feature_,
             IPHFailureReason reason_,
             const feature_engagement::FeatureConfig* config_)
      : feature(feature_), reason(reason_), config(config_) {}
  IPHFailure(const IPHFailure& other) = default;
  IPHFailure& operator=(const IPHFailure& other) = default;

  raw_ptr<const base::Feature> feature = nullptr;
  IPHFailureReason reason = IPHFailureReason::kNone;
  raw_ptr<const feature_engagement::FeatureConfig> config = nullptr;
};

std::ostream& operator<<(std::ostream& os,
                         feature_engagement::ComparatorType type) {
  switch (type) {
    case feature_engagement::ANY:
      os << "ANY";
      break;
    case feature_engagement::LESS_THAN:
      os << "LESS_THAN";
      break;
    case feature_engagement::GREATER_THAN:
      os << "GREATER_THAN";
      break;
    case feature_engagement::LESS_THAN_OR_EQUAL:
      os << "LESS_THAN_OR_EQUAL";
      break;
    case feature_engagement::GREATER_THAN_OR_EQUAL:
      os << "GREATER_THAN_OR_EQUAL";
      break;
    case feature_engagement::EQUAL:
      os << "EQUAL";
      break;
    case feature_engagement::NOT_EQUAL:
      os << "NOT_EQUAL";
      break;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         feature_engagement::SessionRateImpact::Type type) {
  switch (type) {
    case feature_engagement::SessionRateImpact::Type::ALL:
      os << "ALL";
      break;
    case feature_engagement::SessionRateImpact::Type::EXPLICIT:
      os << "EXPLICIT";
      break;
    case feature_engagement::SessionRateImpact::Type::NONE:
      os << "NONE";
      break;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const IPHFailure& failure) {
  os << failure.feature->name;
  switch (failure.reason) {
    case IPHFailureReason::kNone:
      NOTREACHED_IN_MIGRATION();
      break;
    case IPHFailureReason::kUnlisted:
      os << " is not registered in feature_engagement::kAllFeatures in "
            "feature_list.cc. This will cause most attempts to show or access "
            "data about the feature to crash. Please add it.";
      break;
    case IPHFailureReason::kWrongSessionRate:
      os << " has unexpected session rate: "
         << failure.config->session_rate.type << ", "
         << failure.config->session_rate.value
         << ". Non-toast IPH should have limited session_rate - typically "
            "EQUALS, 0 - to prevent other IPH from running in the same "
            "session.";
      break;
    case IPHFailureReason::kWrongSessionImpact:
      os << " has unexpected session rate impact: "
         << failure.config->session_rate_impact.type
         << ". An IPH which runs once per session should also prevent other "
            "similar IPH from running (session rate impact ALL); an IPH which "
            "is not limited should not (session rate impact NONE).";
      break;
    case IPHFailureReason::kWrongSessionImpactToast:
      os << " has unexpected per-app session rate and/or session rate impact: "
         << failure.config->session_rate.type << ", "
         << failure.config->session_rate.value << ", "
         << failure.config->session_rate_impact.type
         << ". Toast promos should never be prevented from "
            "running (session rate ANY) and should not prevent other IPH from "
            "running (session rate impact NONE).";
      break;
    case IPHFailureReason::kWrongSessionImpactKeyedNotice:
      os << " has unexpected per-key session rate and/or session rate impact: "
         << failure.config->session_rate.type << ", "
         << failure.config->session_rate.value << ", "
         << failure.config->session_rate_impact.type
         << ". A heavyweight keyed notice should never be prevented from "
            "running (session rate ANY) but should prevent other IPH from "
            "running (session rate impact ALL).";
      break;
    case IPHFailureReason::kWrongSessionImpactLegalNotice:
      os << " has unexpected per-app session rate and/or session rate impact: "
         << failure.config->session_rate.type << ", "
         << failure.config->session_rate.value << ", "
         << failure.config->session_rate_impact.type
         << ". A heavyweight legal notice should never be prevented from "
            "running (session rate ANY) but should prevent other IPH from "
            "running (session rate impact ALL).";
      break;
    case IPHFailureReason::kWrongSessionImpactActionableAlert:
      os << " has unexpected per-app session rate and/or session rate impact: "
         << failure.config->session_rate.type << ", "
         << failure.config->session_rate.value << ", "
         << failure.config->session_rate_impact.type
         << ". An actionable alert should never be prevented from "
            "running (session rate ANY) but should prevent other IPH from "
            "running (session rate impact ALL).";
      break;
    case IPHFailureReason::kLegacyPromoNoScreenReader:
      os << " is a legacy promo with inadequate screen reader support. Use a "
            "toast promo instead.";
      break;
    case IPHFailureReason::kWrongSessionParamsRotatingPromo:
      os << " has unexpected session rate and/or session rate impact: "
         << failure.config->session_rate.type << ", "
         << failure.config->session_rate.value << ", "
         << failure.config->session_rate_impact.type
         << ". A rotating promo should never be prevented from running "
            "(session rate ANY) and should not prevent other IPH from "
            "running (session rate impact NONE).";
      break;
  }
  return os;
}

template <typename T, typename U>
void MaybeAddFailure(T& failures,
                     const U& exceptions,
                     const base::Feature* feature,
                     IPHFailureReason reason,
                     const feature_engagement::FeatureConfig* feature_config) {
  IPHFailure failure(feature, reason, feature_config);
  for (const auto& exception : exceptions) {
    if (exception.feature == feature) {
      if (!exception.reason.has_value() || exception.reason.value() == reason) {
        LOG(WARNING) << "Allowed by exception or currently being worked - "
                     << exception.description << ":\n"
                     << failure;
      }
      return;
    }
  }
  failures.push_back(failure);
}

template <typename T>
std::string FailuresToString(const T& failures, const char* type) {
  std::ostringstream oss;
  oss << "\nNOTE TO GARDENERS:\n"
         "This test validates the configurations of "
      << type
      << "s in browser_user_education_service.cc, feature_configurations.cc, "
         "and/or fieldtrial_testing_config.json. If this test fails, it is "
         "likely not because this test is faulty, but because an invalid "
         "configuration has somehow snuck past CQ.\n\n"
         "The failed configurations will be listed below. The feature names "
         "listed will help you track down which CL may have caused the error. "
         "Please do not disable this test. Instead, locate the offending CL "
         "and revert that, or tag a bug to its author if it cannot be reverted."
      << "\n\nErrors found during " << type << " configuration validation:";
  for (auto& failure : failures) {
    oss << "\n" << failure;
  }
  return oss.str();
}

bool IsComparatorLimited(const feature_engagement::Comparator& comparator,
                         uint32_t max_count) {
  switch (comparator.type) {
    case feature_engagement::ANY:
    case feature_engagement::GREATER_THAN:
    case feature_engagement::GREATER_THAN_OR_EQUAL:
    case feature_engagement::NOT_EQUAL:
      return false;
    case feature_engagement::LESS_THAN:
      return comparator.value <= max_count;
    case feature_engagement::LESS_THAN_OR_EQUAL:
    case feature_engagement::EQUAL:
      return comparator.value < max_count;
  }
}

}  // namespace

using BrowserUserEducationServiceBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(BrowserUserEducationServiceBrowserTest,
                       FeatureConfigurationConsistencyCheck) {
  // Exceptions to the consistency checks. All of those with crbug.com IDs
  // should ideally be fixed. See tracking bug at crbug.com/1442977
  const std::vector<IPHException> exceptions({
      // Known weird/old/test-only IPH.
      {&feature_engagement::kIPHAutofillExternalAccountProfileSuggestionFeature,
       IPHFailureReason::kLegacyPromoNoScreenReader, "Known legacy promo."},
      {&feature_engagement::kIPHAutofillVirtualCardSuggestionFeature,
       IPHFailureReason::kLegacyPromoNoScreenReader, "Known legacy promo."},
      {&feature_engagement::kIPHGMCCastStartStopFeature,
       IPHFailureReason::kLegacyPromoNoScreenReader, "Known legacy promo."},
      {&feature_engagement::kIPHDesktopPwaInstallFeature,
       IPHFailureReason::kLegacyPromoNoScreenReader, "crbug.com/1443016"},
      {&feature_engagement::kIPHReadingListDiscoveryFeature,
       IPHFailureReason::kLegacyPromoNoScreenReader, "crbug.com/1443020"},
      {&feature_engagement::kIPHDesktopSharedHighlightingFeature,
       IPHFailureReason::kLegacyPromoNoScreenReader, "crbug.com/1443071"},

      // Toast IPH that probably need session impact updated.
      {&feature_engagement::kIPHPasswordsManagementBubbleAfterSaveFeature,
       IPHFailureReason::kWrongSessionImpact, "crbug.com/1442979"},
      {&feature_engagement::kIPHPasswordsManagementBubbleDuringSigninFeature,
       IPHFailureReason::kWrongSessionImpact, "crbug.com/1442979"},
      {&feature_engagement::kIPHPasswordsWebAppProfileSwitchFeature,
       IPHFailureReason::kWrongSessionImpact, "crbug.com/1442979"},
      {&feature_engagement::kIPHProfileSwitchFeature,
       IPHFailureReason::kWrongSessionImpact, "crbug.com/1442979"},
      {&feature_engagement::kIPHTabAudioMutingFeature,
       IPHFailureReason::kWrongSessionImpact, "crbug.com/1442979"},

      // IPH that limit session rate in other ways. These should probably be
      // revisited in the future.
      {&feature_engagement::kIPHDesktopCustomizeChromeFeature,
       IPHFailureReason::kWrongSessionRate, "crbug.com/1443063"},
      {&feature_engagement::kIPHDesktopCustomizeChromeRefreshFeature,
       IPHFailureReason::kWrongSessionRate, "crbug.com/1443063"},
      {&feature_engagement::kIPHDesktopCustomizeChromeRefreshFeature,
       IPHFailureReason::kWrongSessionImpact, "crbug.com/1443063"},
      {&feature_engagement::kIPHMemorySaverModeFeature,
       IPHFailureReason::kWrongSessionRate, "crbug.com/1443063"},
      {&feature_engagement::kIPHPriceTrackingInSidePanelFeature, std::nullopt,
       "crbug.com/1443063"},
      {&feature_engagement::kIPHPowerBookmarksSidePanelFeature,
       IPHFailureReason::kWrongSessionRate,
       "crbug.com/1443067, crbug.com/1443063"},

      // Deprecated; should probably be removed.
      {&feature_engagement::kIPHReadingListInSidePanelFeature, std::nullopt,
       "crbug.com/1443078"},
      {&feature_engagement::kIPHTabSearchFeature, std::nullopt,
       "crbug.com/1443079"},
      {&feature_engagement::kIPHWebUITabStripFeature, std::nullopt,
       "crbug.com/1443082"},
  });

  // Fetch the list of known IPH from the Feature Engagement system; it is an
  // error to fail to register an IPH in this list.
  const base::flat_set<const base::Feature*> known_features(
      feature_engagement::GetAllFeatures());

  // Fetch the tracker and ensure that it is properly initialized.
  auto* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(
          browser()->profile());
  base::RunLoop run_loop;
  tracker->AddOnInitializedCallback(base::BindOnce(
      [](base::OnceClosure callback, bool success) {
        ASSERT_TRUE(success);
        std::move(callback).Run();
      },
      run_loop.QuitClosure()));
  run_loop.Run();

  // Get the configuration from the tracker.
  const feature_engagement::Configuration* const configuration =
      tracker->GetConfigurationForTesting();
  ASSERT_NE(nullptr, configuration);

  // Get the associated feature promo registry.
  const user_education::FeaturePromoRegistry& registry =
      UserEducationServiceFactory::GetForBrowserContext(browser()->profile())
          ->feature_promo_registry();

  std::vector<IPHFailure> failures;

  // Iterate through registered IPH and ensure that the configurations are
  // consistent.
  for (const auto& [feature, spec] : registry.feature_data()) {
    // If the feature is not on the known features list, no configuration is
    // possible.
    if (!base::Contains(known_features, feature)) {
      failures.emplace_back(feature, IPHFailureReason::kUnlisted, nullptr);
      continue;
    }

    const feature_engagement::FeatureConfig* feature_config =
        &configuration->GetFeatureConfig(*feature);

    // Fetch the configuration for the given feature.
    std::optional<feature_engagement::FeatureConfig> client_config;
    if (!feature_config->valid) {
      // Disabled features don't read from feature_configurations.cc by default;
      // we have to do it manually to ensure that if Finch enables the feature
      // the configuration we read will be correct.
      client_config = feature_engagement::GetClientSideFeatureConfig(feature);
      ASSERT_TRUE(client_config.has_value())
          << "Auto-configuration failed for \"" << feature->name
          << "\" - this should never happen and is not a normal failure.";
    }

    const bool limits_other_iph =
        feature_config->session_rate_impact.type ==
        feature_engagement::SessionRateImpact::Type::ALL;
    const bool is_session_limited =
        IsComparatorLimited(feature_config->session_rate, 1);
    const bool is_v2 = user_education::features::IsUserEducationV2();

    switch (spec.promo_type()) {
      case user_education::FeaturePromoSpecification::PromoType::kToast:
        // Toast promos are allowed to bypass session exclusivity. However, they
        // should not limit other IPH.
        if (is_session_limited || limits_other_iph) {
          MaybeAddFailure(failures, exceptions, feature,
                          IPHFailureReason::kWrongSessionImpactToast,
                          feature_config);
        }
        break;
      case user_education::FeaturePromoSpecification::PromoType::kTutorial:
      case user_education::FeaturePromoSpecification::PromoType::kCustomAction:
      case user_education::FeaturePromoSpecification::PromoType::kSnooze:
        switch (spec.promo_subtype()) {
          case user_education::FeaturePromoSpecification::PromoSubtype::kNormal:
            // Standard promos should be session-limited and should limit other
            // IPH.
            if (is_v2 == is_session_limited) {
              MaybeAddFailure(failures, exceptions, feature,
                              IPHFailureReason::kWrongSessionRate,
                              feature_config);
            }
            if (!limits_other_iph) {
              MaybeAddFailure(failures, exceptions, feature,
                              IPHFailureReason::kWrongSessionImpact,
                              feature_config);
            }
            break;
          case user_education::FeaturePromoSpecification::PromoSubtype::
              kKeyedNotice:
            // These can be session limited or not, but they should preclude
            // other IPH.
            if (is_session_limited || !limits_other_iph) {
              MaybeAddFailure(failures, exceptions, feature,
                              IPHFailureReason::kWrongSessionImpactKeyedNotice,
                              feature_config);
            }
            break;
          case user_education::FeaturePromoSpecification::PromoSubtype::
              kLegalNotice:
            // These should not be session limited, and should limit other IPH.
            if (is_session_limited || !limits_other_iph) {
              MaybeAddFailure(failures, exceptions, feature,
                              IPHFailureReason::kWrongSessionImpactLegalNotice,
                              feature_config);
            }
            break;
          case user_education::FeaturePromoSpecification::PromoSubtype::
              kActionableAlert:
            // These should not be session limited, and should limit other IPH.
            if (is_session_limited || !limits_other_iph) {
              MaybeAddFailure(
                  failures, exceptions, feature,
                  IPHFailureReason::kWrongSessionImpactActionableAlert,
                  feature_config);
            }
            break;
        }
        break;
      case user_education::FeaturePromoSpecification::PromoType::kLegacy:
      case user_education::FeaturePromoSpecification::PromoType::kUnspecified:
        // Legacy promos are inherently bad. Use toast or snooze instead.
        if (!spec.screen_reader_string_id()) {
          MaybeAddFailure(failures, exceptions, feature,
                          IPHFailureReason::kLegacyPromoNoScreenReader,
                          feature_config);
        }
        // Legacy promos should pattern as snooze or toast promos.
        if (is_session_limited != limits_other_iph) {
          MaybeAddFailure(failures, exceptions, feature,
                          IPHFailureReason::kWrongSessionImpact,
                          feature_config);
        }
        break;
      case user_education::FeaturePromoSpecification::PromoType::kRotating:
        // Rotating promos should be unlimited and not limit other IPH.
        if (is_session_limited || limits_other_iph) {
          MaybeAddFailure(failures, exceptions, feature,
                          IPHFailureReason::kWrongSessionParamsRotatingPromo,
                          feature_config);
        }
        break;
    }
  }

  EXPECT_TRUE(failures.empty()) << FailuresToString(failures, "IPH");
}

namespace {

enum class TutorialFailureReason {
  kNone,
  kLikelySkippedStep,
  kWaitForAlwaysVisibleElement,
};

struct TutorialFailure {
  user_education::TutorialIdentifier tutorial_id;
  int step_number = -1;
  ui::ElementIdentifier identifier;
  TutorialFailureReason reason = TutorialFailureReason::kNone;
};

std::ostream& operator<<(std::ostream& os, const TutorialFailure& failure) {
  os << failure.tutorial_id;
  switch (failure.reason) {
    case TutorialFailureReason::kNone:
      NOTREACHED_IN_MIGRATION();
      break;
    case TutorialFailureReason::kLikelySkippedStep:
      os << " shows a bubble anchored to an always-visible UI element "
         << failure.identifier << " (step " << failure.step_number
         << ") immediately after another bubble. This is likely to cause the "
            " previous step to be skipped, as the transition will be "
            "instantaneous. Please insert a hidden step between these steps "
            "that detects the action you expect the user to take to advance "
            "the tutorial (e.g. an activation step for a button press, or an "
            "event step for the result of some process).";
      break;
    case TutorialFailureReason::kWaitForAlwaysVisibleElement:
      os << " is waiting for element " << failure.identifier
         << " to become visible in the current context (step "
         << failure.step_number
         << "), and is set to only show on state change (i.e. not visible -> "
            "visible). However, this element is already always visible, so the "
            "bubble will likely never show. If you did not intend to wait for "
            "a state transition, make sure `transition_only_on_event` is "
            "false. If you were waiting for another window to appear, make "
            "sure that `context_mode` is ContextMode::kAny.";
      break;
  }
  return os;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(BrowserUserEducationServiceBrowserTest,
                       TutorialConsistencyCheck) {
  const base::flat_set<ui::ElementIdentifier> kAlwaysPresentElementIds = {
      kToolbarAppMenuButtonElementId,
      kToolbarAvatarButtonElementId,
      kToolbarBackButtonElementId,
      kBrowserViewElementId,
      kToolbarForwardButtonElementId,
      kNewTabButtonElementId,
      kOmniboxElementId,
      kToolbarSidePanelButtonElementId,
      kTabSearchButtonElementId,
      kTabStripElementId,
      kTabStripRegionElementId,
      kTopContainerElementId};

  std::vector<TutorialFailure> failures;

  auto* const service =
      UserEducationServiceFactory::GetForBrowserContext(browser()->profile());
  const auto& registry = service->tutorial_registry();
  for (auto identifier : registry.GetTutorialIdentifiers()) {
    const auto* const description = registry.GetTutorialDescription(identifier);
    bool was_show_bubble = false;
    int step_count = 0;
    for (const auto& step : description->steps) {
      ++step_count;
      const bool is_show_bubble =
          (step.step_type() == ui::InteractionSequence::StepType::kShown &&
           step.body_text_id());
      const bool is_always_visible =
          base::Contains(kAlwaysPresentElementIds, step.element_id());
      if (is_show_bubble && was_show_bubble && is_always_visible &&
          !step.transition_only_on_event()) {
        failures.push_back(
            TutorialFailure{identifier, step_count, step.element_id(),
                            TutorialFailureReason::kLikelySkippedStep});
      } else if (is_always_visible && step.transition_only_on_event() &&
                 step.context_mode() !=
                     ui::InteractionSequence::ContextMode::kAny) {
        failures.push_back(TutorialFailure{
            identifier, step_count, step.element_id(),
            TutorialFailureReason::kWaitForAlwaysVisibleElement});
      }
      was_show_bubble = is_show_bubble;
    }
  }

  EXPECT_TRUE(failures.empty()) << FailuresToString(failures, "Tutorial");
}

IN_PROC_BROWSER_TEST_F(BrowserUserEducationServiceBrowserTest, AutoConfigure) {
  auto* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(
          browser()->profile());
  const auto& config = tracker->GetConfigurationForTesting()->GetFeatureConfig(
      feature_engagement::kIPHWebUiHelpBubbleTestFeature);

  EXPECT_TRUE(config.valid);

  EXPECT_EQ(feature_engagement::EventConfig(
                "WebUiHelpBubbleTest_used",
                feature_engagement::Comparator(feature_engagement::EQUAL, 0),
                feature_engagement::kMaxStoragePeriod,
                feature_engagement::kMaxStoragePeriod),
            config.used);
  EXPECT_EQ(feature_engagement::EventConfig(
                "WebUiHelpBubbleTest_trigger",
                user_education::features::IsUserEducationV2()
                    ? feature_engagement::Comparator(feature_engagement::ANY, 0)
                    : feature_engagement::Comparator(
                          feature_engagement::LESS_THAN, 5),
                feature_engagement::kMaxStoragePeriod,
                feature_engagement::kMaxStoragePeriod),
            config.trigger);
  EXPECT_TRUE(config.event_configs.empty());
  EXPECT_EQ(user_education::features::IsUserEducationV2()
                ? feature_engagement::Comparator(feature_engagement::ANY, 0)
                : feature_engagement::Comparator(feature_engagement::EQUAL, 0),
            config.session_rate);
  EXPECT_EQ(feature_engagement::SessionRateImpact::Type::ALL,
            config.session_rate_impact.type);
  EXPECT_EQ(feature_engagement::BlockedBy(), config.blocked_by);
  EXPECT_EQ(feature_engagement::Blocking(), config.blocking);
  EXPECT_EQ(feature_engagement::Comparator(feature_engagement::ANY, 0),
            config.availability);
  EXPECT_FALSE(config.tracking_only);
  EXPECT_EQ(feature_engagement::SnoozeParams(), config.snooze_params);
  EXPECT_TRUE(config.groups.empty());
}

class BrowserUserEducationServiceNewBadgeBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  BrowserUserEducationServiceNewBadgeBrowserTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          user_education::features::kNewBadgeTestFeature);
    }
  }

  ~BrowserUserEducationServiceNewBadgeBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Make this seem like an old profile so we are not in the new profile
    // grace period.
    auto& storage_service =
        UserEducationServiceFactory::GetForBrowserContext(browser()->profile())
            ->feature_promo_storage_service();
    storage_service.set_profile_creation_time_for_testing(
        storage_service.GetCurrentTime() - base::Days(365));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         BrowserUserEducationServiceNewBadgeBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(BrowserUserEducationServiceNewBadgeBrowserTest,
                       ShowsNewBadge) {
  // Ensure both ways to check the badge work as expected.
  EXPECT_EQ(GetParam(), browser()->window()->MaybeShowNewBadgeFor(
                            user_education::features::kNewBadgeTestFeature));
  EXPECT_EQ(GetParam(), UserEducationService::MaybeShowNewBadge(
                            browser()->profile(),
                            user_education::features::kNewBadgeTestFeature));

  // Ensure that the feature can be marked as used.
  for (int i = 0; i < user_education::features::GetNewBadgeFeatureUsedCount();
       i += 2) {
    browser()->window()->NotifyNewBadgeFeatureUsed(
        user_education::features::kNewBadgeTestFeature);
    UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
        browser()->profile(), user_education::features::kNewBadgeTestFeature);
  }

  // The badge should now be blocked.
  EXPECT_FALSE(browser()->window()->MaybeShowNewBadgeFor(
      user_education::features::kNewBadgeTestFeature));
  EXPECT_FALSE(UserEducationService::MaybeShowNewBadge(
      browser()->profile(), user_education::features::kNewBadgeTestFeature));
}

IN_PROC_BROWSER_TEST_P(BrowserUserEducationServiceNewBadgeBrowserTest,
                       IncognitoDoesNotShowBadge) {
  // Both ways to check the badge should return false for an OTR profile.
  auto* const incog = CreateIncognitoBrowser();
  EXPECT_FALSE(incog->window()->MaybeShowNewBadgeFor(
      user_education::features::kNewBadgeTestFeature));
  EXPECT_FALSE(UserEducationService::MaybeShowNewBadge(
      incog->profile(), user_education::features::kNewBadgeTestFeature));

  // Ensure that the feature can be marked as used.
  for (int i = 0; i < user_education::features::GetNewBadgeFeatureUsedCount();
       i += 2) {
    browser()->window()->NotifyNewBadgeFeatureUsed(
        user_education::features::kNewBadgeTestFeature);
    UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
        browser()->profile(), user_education::features::kNewBadgeTestFeature);
  }

  // The badge should still be blocked.
  EXPECT_FALSE(incog->window()->MaybeShowNewBadgeFor(
      user_education::features::kNewBadgeTestFeature));
  EXPECT_FALSE(UserEducationService::MaybeShowNewBadge(
      incog->profile(), user_education::features::kNewBadgeTestFeature));
}

// Tests for the presence or absence of the recent sessions logic based on
// the enabling flag.
class BrowserUserEducationServiceRecentSessionsTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  BrowserUserEducationServiceRecentSessionsTest() = default;
  ~BrowserUserEducationServiceRecentSessionsTest() override = default;

  void SetUp() override {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(kAllowRecentSessionTracking);
    } else {
      feature_list_.InitAndDisableFeature(kAllowRecentSessionTracking);
    }
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         BrowserUserEducationServiceRecentSessionsTest,
                         testing::Bool());

// Ensure that the recent sessions logic only gets created if the flag is
// enabled.
IN_PROC_BROWSER_TEST_P(BrowserUserEducationServiceRecentSessionsTest,
                       RecentSessionTrackerDependsOnFlag) {
  auto* const result =
      UserEducationServiceFactory::GetForBrowserContext(browser()->profile())
          ->recent_session_tracker();
  EXPECT_EQ(GetParam(), result != nullptr);
}

// Verify that the "disable rate limiting" command line arg works.
class BrowserUserEducationServiceCommandLineTest
    : public BrowserUserEducationServiceBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam()) {
      command_line->AppendSwitch(
          user_education::features::kDisableRateLimitingCommandLine);
    }
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         BrowserUserEducationServiceCommandLineTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(BrowserUserEducationServiceCommandLineTest,
                       DisableUserEducationRateLimiting) {
  if (GetParam()) {
    EXPECT_EQ(user_education::features::GetLowPriorityCooldown(),
              base::Seconds(0));
    EXPECT_EQ(user_education::features::GetSessionStartGracePeriod(),
              base::Seconds(0));
    EXPECT_EQ(user_education::features::GetNewProfileGracePeriod(),
              base::Seconds(0));
  } else {
    EXPECT_GT(user_education::features::GetLowPriorityCooldown(),
              base::Seconds(0));
    EXPECT_GT(user_education::features::GetSessionStartGracePeriod(),
              base::Seconds(0));
    EXPECT_GT(user_education::features::GetNewProfileGracePeriod(),
              base::Seconds(0));
  }
}
