// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/feature_promo_specification.h"

#include <string>
#include <variant>

#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"

namespace user_education {

namespace {

// This function provides the list of allowed legal promos.
// It is not to be modified except by the Frizzle team.
bool IsAllowedLegalNotice(const base::Feature& promo_feature) {
  // Add the text names of allowlisted critical promos here:
  // static constexpr auto kAllowedPromoNames =
  //     base::MakeFixedFlatSet<std::string_view>({ });
  // return kAllowedPromoNames.contains(promo_feature.name);
  return false;
}

bool IsAllowedActionableAlert(const base::Feature& promo_feature) {
  // Add the text names of allowlisted actionable alerts here:
  static constexpr auto kAllowedPromoNames =
      base::MakeFixedFlatSet<std::string_view>({
          "IPH_DownloadEsbPromo",
          "IPH_HighEfficiencyMode",
          "IPH_iOSEnhancedBrowsingDesktop",
          "IPH_SignInBenefits",
          "IPH_SupervisedUserProfileSignin",
          "IPH_TabSearchToolbarButton",
      });
  return kAllowedPromoNames.contains(promo_feature.name);
}

bool IsAllowedKeyedNotice(const base::Feature& promo_feature) {
  // Add the text names of allowlisted keyed notices here:
  static constexpr auto kAllowedPromoNames =
      base::MakeFixedFlatSet<std::string_view>({
          "IPH_DesktopPWAsLinkCapturingLaunch",
          "IPH_DesktopPWAsLinkCapturingLaunchAppInTab",
          "IPH_ExplicitBrowserSigninPreferenceRemembered",
          "IPH_PwaQuietNotification",
      });
  return kAllowedPromoNames.contains(promo_feature.name);
}

bool IsAllowedRotatingPromo(const base::Feature& promo_feature) {
  // Add the text names of allowlisted rotating promos here:
  // static constexpr auto kAllowedPromoNames =
  //     base::MakeFixedFlatSet<std::string_view>({ });
  // return kAllowedPromoNames.contains(promo_feature.name);
  return false;
}

bool IsAllowedCustomUiPromo(const base::Feature& promo_feature) {
  // Test-only features are allowed.
  if (std::string(promo_feature.name).starts_with("TEST_")) {
    return true;
  }

  // IMPORTANT NOTE: Because Custom UI promos can potentially violate
  // best-practice rules, be sure to include a link to a screenshot or mock of
  // your UI with a description of how it works in your CL in addition to
  // requesting an exception here.
  //
  // Or better yet, reach out to Frizzle (User Education) Team before adding
  // code for your new Custom UI promo.
  //
  // Add the text names of allowlisted rotating promos here:
  static constexpr auto kAllowedPromoNames =
      base::MakeFixedFlatSet<std::string_view>(
          {"IPH_ExtensionsZeroStatePromo", "IPH_iOSEnhancedBrowsingDesktop",
           "IPH_iOSLensPromoDesktop", "IPH_iOSPasswordPromoDesktop"});
  return kAllowedPromoNames.contains(promo_feature.name);
}

bool IsAllowedLegacyPromo(const base::Feature& promo_feature) {
  // NOTE: LEGACY PROMOS ARE DEPRECATED.
  // NO NEW ITEMS SHOULD BE ADDED TO THIS LIST, EVER.
  static constexpr auto kAllowedPromoNames =
      base::MakeFixedFlatSet<std::string_view>({
          "IPH_AutofillExternalAccountProfileSuggestion",
          "IPH_AutofillVirtualCardSuggestion",
          "IPH_DesktopPwaInstall",
          "IPH_DesktopSharedHighlighting",
          "IPH_GMCCastStartStop",
          "IPH_PriceTrackingInSidePanel",
          "IPH_ReadingListDiscovery",
          "IPH_ReadingListInSidePanel",
          "IPH_TabSearch",
      });
  return kAllowedPromoNames.contains(promo_feature.name);
}

bool IsAllowedToastWithoutScreenreaderText(const base::Feature& promo_feature) {
  // Features used for tests have this prefix and are excluded.
  if (std::string_view(promo_feature.name).starts_with("TEST_")) {
    return true;
  }

  // Some toasts are purely informational and their normal text also works for
  // low-vision users. This is a very small percentage of toasts, and so only
  // specific such promos are allowlisted.
  //
  // TODO(dfried): Merge legacy promos into this category, eliminating the entry
  // point and promo type entirely.

  // TODO(crbug.com/421471598): Remove this exemption once we have a separate
  // string for screenreader.
  static constexpr auto kAllowedPromoNames =
      base::MakeFixedFlatSet<std::string_view>({"IPH_TabSearchToolbarButton"});
  return kAllowedPromoNames.contains(promo_feature.name);
}

bool IsAllowedPreconditionExemption(const base::Feature& promo_feature) {
  // Features used for tests have this prefix and are excluded.
  if (std::string_view(promo_feature.name).starts_with("TEST_")) {
    return true;
  }

  static constexpr auto kAllowedPromoNames =
      base::MakeFixedFlatSet<std::string_view>({"IPH_AutofillAiOptIn"});
  return kAllowedPromoNames.contains(promo_feature.name);
}

// Common check logic for gating reshow-ability of promos. Generates an error if
// `subtype` is not allowed to reshow.
void CheckReshowAllowedFor(FeaturePromoSpecification::PromoSubtype subtype) {
  CHECK(subtype == FeaturePromoSpecification::PromoSubtype::kKeyedNotice ||
        subtype == FeaturePromoSpecification::PromoSubtype::kLegalNotice)
      << "Reshow only allowed for certain promo subtypes; subtype was "
      << subtype;
}

// Gets the minimum allowed reshow delay for specific promo types and
// `subtypes`.
//
// Note that currently, the subtype parameter does not affect the delay; it is
// here for sanity checking and so we can tune policy later if necessary.
base::TimeDelta GetMinReshowDelay(
    FeaturePromoSpecification::PromoType type,
    FeaturePromoSpecification::PromoSubtype subtype) {
  CheckReshowAllowedFor(subtype);
  if (type == FeaturePromoSpecification::PromoType::kToast) {
    return base::Days(14);
  } else {
    return base::Days(90);
  }
}

}  // namespace

FeaturePromoSpecification::AdditionalConditions::AdditionalConditions() =
    default;
FeaturePromoSpecification::AdditionalConditions::AdditionalConditions(
    AdditionalConditions&&) noexcept = default;
FeaturePromoSpecification::AdditionalConditions&
FeaturePromoSpecification::AdditionalConditions::operator=(
    AdditionalConditions&&) noexcept = default;
FeaturePromoSpecification::AdditionalConditions::~AdditionalConditions() =
    default;

FeaturePromoSpecification::AdditionalConditions&
FeaturePromoSpecification::AdditionalConditions::AddAdditionalCondition(
    const AdditionalCondition& additional_condition) {
  additional_conditions_.emplace_back(additional_condition);
  return *this;
}

void FeaturePromoSpecification::AdditionalConditions::AddAdditionalCondition(
    const char* event_name,
    Constraint constraint,
    uint32_t count,
    std::optional<uint32_t> in_days) {
  AddAdditionalCondition({event_name, constraint, count, in_days});
}

FeaturePromoSpecification::AcceleratorInfo::AcceleratorInfo() = default;
FeaturePromoSpecification::AcceleratorInfo::AcceleratorInfo(
    const AcceleratorInfo& other) = default;
FeaturePromoSpecification::AcceleratorInfo::AcceleratorInfo(ValueType value)
    : value_(value) {}
FeaturePromoSpecification::AcceleratorInfo&
FeaturePromoSpecification::AcceleratorInfo::operator=(
    const AcceleratorInfo& other) = default;
FeaturePromoSpecification::AcceleratorInfo::~AcceleratorInfo() = default;

FeaturePromoSpecification::AcceleratorInfo&
FeaturePromoSpecification::AcceleratorInfo::operator=(ValueType value) {
  value_ = value;
  return *this;
}

FeaturePromoSpecification::AcceleratorInfo::operator bool() const {
  return std::holds_alternative<ui::Accelerator>(value_) ||
         std::get<int>(value_);
}

ui::Accelerator FeaturePromoSpecification::AcceleratorInfo::GetAccelerator(
    const ui::AcceleratorProvider* provider) const {
  if (std::holds_alternative<ui::Accelerator>(value_)) {
    return std::get<ui::Accelerator>(value_);
  }

  const int command_id = std::get<int>(value_);
  DCHECK_GT(command_id, 0);

  ui::Accelerator result;
  DCHECK(provider->GetAcceleratorForCommandId(command_id, &result));
  return result;
}

FeaturePromoSpecification::RotatingPromos::RotatingPromos() = default;
FeaturePromoSpecification::RotatingPromos::RotatingPromos(
    RotatingPromos&&) noexcept = default;
FeaturePromoSpecification::RotatingPromos&
FeaturePromoSpecification::RotatingPromos::operator=(
    RotatingPromos&&) noexcept = default;
FeaturePromoSpecification::RotatingPromos::~RotatingPromos() = default;

FeaturePromoSpecification::BuildHelpBubbleParams::BuildHelpBubbleParams() =
    default;
FeaturePromoSpecification::BuildHelpBubbleParams::BuildHelpBubbleParams(
    const BuildHelpBubbleParams&) = default;
FeaturePromoSpecification::BuildHelpBubbleParams::BuildHelpBubbleParams(
    BuildHelpBubbleParams&&) noexcept = default;
FeaturePromoSpecification::BuildHelpBubbleParams&
FeaturePromoSpecification::BuildHelpBubbleParams::operator=(
    const BuildHelpBubbleParams&) = default;
FeaturePromoSpecification::BuildHelpBubbleParams&
FeaturePromoSpecification::BuildHelpBubbleParams::operator=(
    BuildHelpBubbleParams&&) noexcept = default;
FeaturePromoSpecification::BuildHelpBubbleParams::~BuildHelpBubbleParams() =
    default;

// static
constexpr HelpBubbleArrow FeaturePromoSpecification::kDefaultBubbleArrow;

FeaturePromoSpecification::FeaturePromoSpecification() = default;

FeaturePromoSpecification::FeaturePromoSpecification(
    FeaturePromoSpecification&& other) noexcept = default;

FeaturePromoSpecification::FeaturePromoSpecification(
    const base::Feature* feature,
    PromoType promo_type,
    ui::ElementIdentifier anchor_element_id,
    int bubble_body_string_id)
    : AnchorElementProviderCommon(anchor_element_id),
      feature_(feature),
      promo_type_(promo_type),
      bubble_body_string_id_(bubble_body_string_id),
      custom_action_dismiss_string_id_(IDS_PROMO_DISMISS_BUTTON) {
  DCHECK_NE(promo_type, PromoType::kUnspecified);
  DCHECK(promo_type == PromoType::kCustomUi || bubble_body_string_id_ != 0);
}

FeaturePromoSpecification& FeaturePromoSpecification::operator=(
    FeaturePromoSpecification&& other) noexcept = default;

FeaturePromoSpecification::~FeaturePromoSpecification() = default;

std::u16string FeaturePromoSpecification::FormatString(
    int string_id,
    const FormatParameters& format_params) {
  if (!string_id) {
    CHECK(std::holds_alternative<NoSubstitution>(format_params));
    return std::u16string();
  }
  if (std::holds_alternative<NoSubstitution>(format_params)) {
    return l10n_util::GetStringUTF16(string_id);
  }
  if (const auto* substitutions =
          std::get_if<StringSubstitutions>(&format_params)) {
    return l10n_util::GetStringFUTF16(string_id, *substitutions, nullptr);
  }
  if (const std::u16string* str = std::get_if<std::u16string>(&format_params)) {
    return l10n_util::GetStringFUTF16(string_id, *str);
  }
  int number = std::get<int>(format_params);
  return l10n_util::GetPluralStringFUTF16(string_id, number);
}

// static
FeaturePromoSpecification FeaturePromoSpecification::CreateForToastPromo(
    const base::Feature& feature,
    ui::ElementIdentifier anchor_element_id,
    int body_text_string_id,
    int accessible_text_string_id,
    AcceleratorInfo accessible_accelerator) {
  // In the vast majority of cases, separate screenreader text should be
  // included for toasts; this is strictly enforced.
  if (body_text_string_id == accessible_text_string_id ||
      accessible_text_string_id <= 0) {
    CHECK(IsAllowedToastWithoutScreenreaderText(feature))
        << "Because toasts are hard to notice and time out quickly, screen "
           "reader text associated with toasts should differ from the bubble "
           "text and either provide the accelerator to access the highlighted "
           "entry point for your feature, or at the very least provide a "
           "separate description of the screen element appropriate for "
           "keyboard "
           "and low-vision users.";
    if (accessible_text_string_id <= 0) {
      accessible_text_string_id = body_text_string_id;
    }
  }

  FeaturePromoSpecification spec(&feature, PromoType::kToast, anchor_element_id,
                                 body_text_string_id);
  spec.screen_reader_string_id_ = accessible_text_string_id;
  spec.screen_reader_accelerator_ = std::move(accessible_accelerator);
  return spec;
}

// static
FeaturePromoSpecification FeaturePromoSpecification::CreateForSnoozePromo(
    const base::Feature& feature,
    ui::ElementIdentifier anchor_element_id,
    int body_text_string_id) {
  return FeaturePromoSpecification(&feature, PromoType::kSnooze,
                                   anchor_element_id, body_text_string_id);
}

// static
FeaturePromoSpecification FeaturePromoSpecification::CreateForSnoozePromo(
    const base::Feature& feature,
    ui::ElementIdentifier anchor_element_id,
    int body_text_string_id,
    int accessible_text_string_id,
    AcceleratorInfo accessible_accelerator) {
  // See `FeaturePromoSpecification::CreateForToastPromo()`.
  CHECK_NE(body_text_string_id, accessible_text_string_id);
  FeaturePromoSpecification spec(&feature, PromoType::kSnooze,
                                 anchor_element_id, body_text_string_id);
  spec.screen_reader_string_id_ = accessible_text_string_id;
  spec.screen_reader_accelerator_ = std::move(accessible_accelerator);
  return spec;
}

// static
FeaturePromoSpecification FeaturePromoSpecification::CreateForTutorialPromo(
    const base::Feature& feature,
    ui::ElementIdentifier anchor_element_id,
    int body_text_string_id,
    TutorialIdentifier tutorial_id) {
  FeaturePromoSpecification spec(&feature, PromoType::kTutorial,
                                 anchor_element_id, body_text_string_id);
  DCHECK(!tutorial_id.empty());
  spec.tutorial_id_ = tutorial_id;
  return spec;
}

// static
FeaturePromoSpecification FeaturePromoSpecification::CreateForCustomAction(
    const base::Feature& feature,
    ui::ElementIdentifier anchor_element_id,
    int body_text_string_id,
    int custom_action_string_id,
    CustomActionCallback custom_action_callback) {
  FeaturePromoSpecification spec(&feature, PromoType::kCustomAction,
                                 anchor_element_id, body_text_string_id);
  spec.custom_action_caption_ =
      l10n_util::GetStringUTF16(custom_action_string_id);
  spec.custom_action_callback_ = custom_action_callback;
  return spec;
}

// static
FeaturePromoSpecification FeaturePromoSpecification::CreateForRotatingPromo(
    const base::Feature& feature,
    RotatingPromos rotating_promos) {
  CHECK(IsAllowedRotatingPromo(feature));
  CHECK_GT(rotating_promos.size(), 0U);
  FeaturePromoSpecification spec;
  spec.feature_ = &feature;
  spec.promo_type_ = PromoType::kRotating;

  // Check the rotating promos to ensure they're all normal promos.
  bool found_rotating_promo = false;
  for (const auto& promo : rotating_promos) {
    if (promo) {
      CHECK_EQ(PromoSubtype::kNormal, promo->promo_subtype())
          << "Rotating promo cannot contain promo of type "
          << promo->promo_type() << " and subtype " << promo->promo_subtype();
      CHECK_NE(PromoType::kLegacy, promo->promo_type())
          << "Rotating promo cannot contain promo of type Legacy";
      CHECK_NE(PromoType::kUnspecified, promo->promo_type())
          << "Rotating promo cannot contain promo of type Unspecified";
      found_rotating_promo = true;
    }
  }
  CHECK(found_rotating_promo);
  spec.rotating_promos_ = std::move(rotating_promos);

  return spec;
}

// static
FeaturePromoSpecification FeaturePromoSpecification::CreateForCustomUi(
    const base::Feature& feature,
    ui::ElementIdentifier anchor_element_id,
    WrappedCustomHelpBubbleFactoryCallback bubble_factory_callback,
    CustomActionCallback custom_action_callback) {
  CHECK(IsAllowedCustomUiPromo(feature));
  FeaturePromoSpecification spec(&feature, PromoType::kCustomUi,
                                 anchor_element_id, 0);
  spec.custom_ui_factory_callback_ = std::move(bubble_factory_callback);
  spec.custom_action_callback_ = std::move(custom_action_callback);
  return spec;
}

// static
FeaturePromoSpecification FeaturePromoSpecification::CreateForLegacyPromo(
    const base::Feature* feature,
    ui::ElementIdentifier anchor_element_id,
    int body_text_string_id) {
  CHECK(!feature || IsAllowedLegacyPromo(*feature))
      << "Cannot create promo: " << feature->name
      << "\nNo new legacy promos may be created; use CreateForToastPromo() "
         "instead.";
  return FeaturePromoSpecification(feature, PromoType::kLegacy,
                                   anchor_element_id, body_text_string_id);
}

// static
FeaturePromoSpecification FeaturePromoSpecification::CreateForTesting(
    const base::Feature& feature,
    ui::ElementIdentifier anchor_element_id,
    int body_text_string_id,
    PromoType type,
    PromoSubtype subtype,
    CustomActionCallback custom_action_callback) {
  FeaturePromoSpecification result(&feature, type, anchor_element_id,
                                   body_text_string_id);
  result.set_promo_subtype_for_testing(subtype);  // IN-TEST
  CHECK_EQ(!custom_action_callback.is_null(), type == PromoType::kCustomAction);
  result.custom_action_callback_ = std::move(custom_action_callback);
  return result;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetBubbleTitleText(
    int title_text_string_id) {
  DCHECK_NE(promo_type_, PromoType::kUnspecified);
  bubble_title_string_id_ = title_text_string_id;
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetBubbleIcon(
    const gfx::VectorIcon* bubble_icon) {
  DCHECK_NE(promo_type_, PromoType::kUnspecified);
  bubble_icon_ = bubble_icon;
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetBubbleArrow(
    HelpBubbleArrow bubble_arrow) {
  CHECK(!bubble_arrow_callback_);
  bubble_arrow_ = bubble_arrow;
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetBubbleArrowCallback(
    HelpBubbleArrowCallback bubble_arrow_callback) {
  CHECK(!bubble_arrow_callback_);
  bubble_arrow_callback_ = std::move(bubble_arrow_callback);
  return *this;
}

HelpBubbleArrow FeaturePromoSpecification::GetBubbleArrow(
    const ui::TrackedElement* anchor_element) const {
  if (bubble_arrow_callback_) {
    return bubble_arrow_callback_.Run(anchor_element);
  }
  return bubble_arrow_;
}

FeaturePromoSpecification& FeaturePromoSpecification::OverrideFocusOnShow(
    bool focus_on_show) {
  focus_on_show_override_ = focus_on_show;
  for (auto& rotating_promo : rotating_promos_) {
    if (rotating_promo.has_value()) {
      rotating_promo->focus_on_show_override_ =
          rotating_promo->focus_on_show_override_.value_or(focus_on_show);
    }
  }
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetPromoSubtype(
    PromoSubtype promo_subtype) {
  CHECK_NE(promo_type_, PromoType::kUnspecified);
  CHECK_NE(promo_type_, PromoType::kRotating)
      << "Rotating is not compatible with other promo subtypes.";
  CHECK_NE(promo_type_, PromoType::kSnooze)
      << "Basic snooze is not compatible with other promo subtypes.";
  CHECK_EQ(promo_subtype_, PromoSubtype::kNormal)
      << "Promo subtype cannot be set multiple times.";
  switch (promo_subtype) {
    case PromoSubtype::kLegalNotice:
      CHECK(feature_);
      CHECK(IsAllowedLegalNotice(*feature_));
      break;
    case PromoSubtype::kActionableAlert:
      CHECK(promo_type_ == PromoType::kCustomAction ||
            promo_type_ == PromoType::kCustomUi);
      CHECK(feature_);
      CHECK(IsAllowedActionableAlert(*feature_));
      break;
    case PromoSubtype::kKeyedNotice:
      CHECK(feature_);
      CHECK(IsAllowedKeyedNotice(*feature_));
      break;
    default:
      break;
  }
  promo_subtype_ = promo_subtype;
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetReshowPolicy(
    base::TimeDelta reshow_delay,
    std::optional<int> max_show_count) {
  CheckReshowAllowedFor(promo_subtype_);
  CHECK_GE(reshow_delay, GetMinReshowDelay(promo_type_, promo_subtype_));
  CHECK(!max_show_count || max_show_count > 1);
  reshow_delay_ = reshow_delay;
  max_show_count_ = max_show_count;
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetAnchorElementFilter(
    AnchorElementFilter anchor_element_filter) {
  set_anchor_element_filter(std::move(anchor_element_filter));
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetInAnyContext(
    bool in_any_context) {
  set_in_any_context(in_any_context);
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetAdditionalConditions(
    AdditionalConditions additional_conditions) {
  additional_conditions_ = std::move(additional_conditions);
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::AddPreconditionExemption(
    FeaturePromoPrecondition::Identifier exempt_precondition) {
  CHECK(IsAllowedPreconditionExemption(*feature_));
  exempt_preconditions_.insert(exempt_precondition);
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetMetadata(
    Metadata metadata) {
  metadata_ = std::move(metadata);
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetCustomActionIsDefault(
    bool custom_action_is_default) {
  DCHECK(custom_action_callback_);
  custom_action_is_default_ = custom_action_is_default;
  return *this;
}

FeaturePromoSpecification&
FeaturePromoSpecification::SetCustomActionDismissText(
    int custom_action_dismiss_string_id) {
  DCHECK_EQ(promo_type_, PromoType::kCustomAction);
  custom_action_dismiss_string_id_ = custom_action_dismiss_string_id;
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetHighlightedMenuItem(
    const ui::ElementIdentifier highlighted_menu_identifier) {
  highlighted_menu_identifier_ = highlighted_menu_identifier;
  return *this;
}

ui::TrackedElement* FeaturePromoSpecification::GetAnchorElement(
    ui::ElementContext context,
    std::optional<int> index) const {
  if (index) {
    CHECK_EQ(PromoType::kRotating, promo_type_);
    return rotating_promos_.at(*index)->GetAnchorElement(context, std::nullopt);
  }

  // Should not be called directly on a rotating promo.
  CHECK_NE(PromoType::kRotating, promo_type_);
  return AnchorElementProviderCommon::GetAnchorElement(context, std::nullopt);
}

int FeaturePromoSpecification::GetNextValidIndex(int starting_index) const {
  CHECK_EQ(PromoType::kRotating, promo_type_);
  int index = starting_index;
  while (!rotating_promos_.at(index).has_value()) {
    index = (index + 1) % rotating_promos_.size();
    CHECK_NE(index, starting_index)
        << "Wrapped around while looking for a valid rotating promo; this "
           "should have been caught during promo registration.";
  }
  return index;
}

// static
FeaturePromoSpecification
FeaturePromoSpecification::CreateRotatingPromoForTesting(
    const base::Feature& feature,
    RotatingPromos rotating_promos) {
  FeaturePromoSpecification spec;
  spec.feature_ = &feature;
  spec.promo_type_ = PromoType::kRotating;
  spec.rotating_promos_ = std::move(rotating_promos);
  return spec;
}

FeaturePromoSpecification::CustomHelpBubbleResult
FeaturePromoSpecification::BuildCustomHelpBubble(
    const UserEducationContextPtr& from_context,
    BuildHelpBubbleParams params) const {
  CHECK_EQ(PromoType::kCustomUi, promo_type_);
  return custom_ui_factory_callback_.Run(from_context, std::move(params));
}

std::ostream& operator<<(std::ostream& oss,
                         FeaturePromoSpecification::PromoType promo_type) {
  switch (promo_type) {
    case FeaturePromoSpecification::PromoType::kLegacy:
      oss << "kLegacy";
      break;
    case FeaturePromoSpecification::PromoType::kToast:
      oss << "kToast";
      break;
    case FeaturePromoSpecification::PromoType::kSnooze:
      oss << "kSnooze";
      break;
    case FeaturePromoSpecification::PromoType::kTutorial:
      oss << "kTutorial";
      break;
    case FeaturePromoSpecification::PromoType::kCustomAction:
      oss << "kCustomAction";
      break;
    case FeaturePromoSpecification::PromoType::kUnspecified:
      oss << "kUnspecified";
      break;
    case FeaturePromoSpecification::PromoType::kRotating:
      oss << "kRotating";
      break;
    case FeaturePromoSpecification::PromoType::kCustomUi:
      oss << "kCustomUi";
      break;
  }
  return oss;
}

std::ostream& operator<<(
    std::ostream& oss,
    FeaturePromoSpecification::PromoSubtype promo_subtype) {
  switch (promo_subtype) {
    case FeaturePromoSpecification::PromoSubtype::kNormal:
      oss << "kNormal";
      break;
    case FeaturePromoSpecification::PromoSubtype::kKeyedNotice:
      oss << "kKeyedNotice";
      break;
    case FeaturePromoSpecification::PromoSubtype::kLegalNotice:
      oss << "kLegalNotice";
      break;
    case FeaturePromoSpecification::PromoSubtype::kActionableAlert:
      oss << "kActionableAlert";
      break;
  }
  return oss;
}

}  // namespace user_education
