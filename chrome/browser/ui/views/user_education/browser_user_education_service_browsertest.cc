// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <map>
#include <ostream>
#include <sstream>
#include <vector>

#include "base/containers/fixed_flat_set.h"
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
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/tutorial_identifier.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace {

enum class IPHFailureReason {
  kNone,
  kNotConfigured,
  kWrongSessionRate,
  kWrongSessionImpact,
  kWrongSessionImpactPerApp,
  kWrongSessionImpactLegalNotice,
  kLegacyPromoNoScreenReader,
};

struct IPHException {
  IPHException() = default;
  IPHException(const base::Feature* feature_,
               absl::optional<IPHFailureReason> reason_,
               const char* description_)
      : feature(feature_), reason(reason_), description(description_) {}
  IPHException(const IPHException& other) = default;
  IPHException& operator=(const IPHException& other) = default;
  ~IPHException() = default;

  raw_ptr<const base::Feature> feature = nullptr;
  absl::optional<IPHFailureReason> reason;
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
      NOTREACHED();
      break;
    case IPHFailureReason::kNotConfigured:
      os << " is not configured. Please add a configuration to "
            "feature_configurations.cc (preferred) or "
            "fieldtrial_testing_config.json.";
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
    case IPHFailureReason::kWrongSessionImpactPerApp:
      os << " has unexpected per-app session rate impact: "
         << failure.config->session_rate_impact.type
         << ". A heavyweight IPH which runs per-app should prevent other IPH "
            "from running (session rate impact ALL); it may or may not be "
            "limited by other IPH.";
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
    case IPHFailureReason::kLegacyPromoNoScreenReader:
      os << " is a legacy promo with inadequate screen reader support. Use a "
            "toast promo instead.";
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
      if ((exception.reason.has_value() &&
           exception.reason.value() == reason) ||
          (reason != IPHFailureReason::kNotConfigured &&
           !exception.reason.has_value())) {
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
  oss << "Errors found during " << type << " configuration validation.";
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
      {&feature_engagement::kIPHWebUiHelpBubbleTestFeature,
       IPHFailureReason::kNotConfigured, "For testing purposes only."},

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
      {&feature_engagement::kIPHDesktopTabGroupsNewGroupFeature,
       IPHFailureReason::kWrongSessionRate, "crbug.com/1443063"},
      {&feature_engagement::kIPHSideSearchFeature,
       IPHFailureReason::kWrongSessionRate, "crbug.com/1443063"},
      {&feature_engagement::kIPHHighEfficiencyModeFeature,
       IPHFailureReason::kWrongSessionRate, "crbug.com/1443063"},
      {&feature_engagement::kIPHPriceTrackingInSidePanelFeature, absl::nullopt,
       "crbug.com/1443063"},
      {&feature_engagement::kIPHPowerBookmarksSidePanelFeature,
       IPHFailureReason::kWrongSessionRate,
       "crbug.com/1443067, crbug.com/1443063"},
      {&feature_engagement::kIPHPasswordsAccountStorageFeature,
       IPHFailureReason::kWrongSessionRate, "crbug.com/1443075"},

      // Deprecated; should probably be removed.
      {&feature_engagement::kIPHReadingListDiscoveryFeature,
       IPHFailureReason::kNotConfigured, "crbug.com/1443020"},
      {&feature_engagement::kIPHReadingListEntryPointFeature,
       IPHFailureReason::kNotConfigured, "crbug.com/1443020"},
      {&feature_engagement::kIPHDesktopSharedHighlightingFeature,
       IPHFailureReason::kNotConfigured, "crbug.com/1443071"},
      {&feature_engagement::kIPHReadingListInSidePanelFeature, absl::nullopt,
       "crbug.com/1443078"},
      {&feature_engagement::kIPHTabSearchFeature, absl::nullopt,
       "crbug.com/1443079"},
      {&feature_engagement::kIPHWebUITabStripFeature, absl::nullopt,
       "crbug.com/1443082"},

      // Needs configuration.
      {&feature_engagement::kIPHLiveCaptionFeature,
       IPHFailureReason::kNotConfigured, "crbug.com/1443002"},
      {&feature_engagement::kIPHBackNavigationMenuFeature,
       IPHFailureReason::kNotConfigured, "crbug.com/1443013"},
      {&feature_engagement::kIPHDesktopPwaInstallFeature,
       IPHFailureReason::kNotConfigured, "crbug.com/1443016"},
  });

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
  for (const auto& [feature, spec] :
       registry.GetRegisteredFeaturePromoSpecifications()) {
    const feature_engagement::FeatureConfig* feature_config =
        &configuration->GetFeatureConfig(*feature);

    // Fetch the configuration for the given feature.
    absl::optional<feature_engagement::FeatureConfig> client_config;
    if (!feature_config->valid) {
      // Disabled features don't read from feature_configurations.cc by default;
      // we have to do it manually to ensure that if Finch enables the feature
      // the configuration we read will be correct.
      client_config = feature_engagement::GetClientSideFeatureConfig(feature);
      if (client_config) {
        feature_config = &client_config.value();
      } else {
        // This is a feature that can only be configured through Finch; current
        // best practice is to also include a fieldtrial or (better) a config
        // in feature_configurations.cc.
        MaybeAddFailure(failures, exceptions, feature,
                        IPHFailureReason::kNotConfigured, feature_config);
        continue;
      }
    }

    const bool limits_other_iph =
        feature_config->session_rate_impact.type ==
        feature_engagement::SessionRateImpact::Type::ALL;
    const bool is_session_limited =
        IsComparatorLimited(feature_config->session_rate, 1);

    switch (spec.promo_type()) {
      case user_education::FeaturePromoSpecification::PromoType::kToast:
        // Toast promos are allowed to bypass session exclusivity. However, they
        // should not limit other IPH.
        if (limits_other_iph) {
          MaybeAddFailure(failures, exceptions, feature,
                          IPHFailureReason::kWrongSessionImpact,
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
            if (!is_session_limited) {
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
          case user_education::FeaturePromoSpecification::PromoSubtype::kPerApp:
            // These can be session limited or not, but they should preclude
            // other IPH.
            if (!limits_other_iph) {
              MaybeAddFailure(failures, exceptions, feature,
                              IPHFailureReason::kWrongSessionImpactPerApp,
                              feature_config);
            }
            break;
          case user_education::FeaturePromoSpecification::PromoSubtype::
              kLegalNotice:
            // These should not be session limited, and should limit other IPH.
            if (is_session_limited || !limits_other_iph) {
              MaybeAddFailure(failures, exceptions, feature,
                              IPHFailureReason::kWrongSessionImpactPerApp,
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
      NOTREACHED();
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
  const auto kAlwaysPresentElementIds =
      base::MakeFixedFlatSet<ui::ElementIdentifier>(
          {kToolbarAppMenuButtonElementId, kToolbarAvatarButtonElementId,
           kToolbarBackButtonElementId, kBrowserViewElementId,
           kToolbarForwardButtonElementId, kNewTabButtonElementId,
           kOmniboxElementId, kToolbarSidePanelButtonElementId,
           kTabSearchButtonElementId, kTabStripElementId,
           kTabStripRegionElementId, kTopContainerElementId});

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
  EXPECT_EQ(
      feature_engagement::EventConfig(
          "WebUiHelpBubbleTest_trigger",
          feature_engagement::Comparator(feature_engagement::LESS_THAN, 3),
          feature_engagement::kMaxStoragePeriod,
          feature_engagement::kMaxStoragePeriod),
      config.trigger);
  EXPECT_TRUE(config.event_configs.empty());
  EXPECT_EQ(feature_engagement::Comparator(feature_engagement::EQUAL, 0),
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
