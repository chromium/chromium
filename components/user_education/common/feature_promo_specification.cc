// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_specification.h"

#include <string>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "components/strings/grit/components_strings.h"
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
  static const char* const kAllowedPromoNames[] = {
      "IPH_TrackingProtectionOnboarding"};
  for (const auto* promo_name : kAllowedPromoNames) {
    if (!strcmp(promo_feature.name, promo_name)) {
      return true;
    }
  }
  return false;
}

}  // namespace

FeaturePromoSpecification::AcceleratorInfo::AcceleratorInfo() = default;
FeaturePromoSpecification::AcceleratorInfo::AcceleratorInfo(
    const AcceleratorInfo& other) = default;
FeaturePromoSpecification::AcceleratorInfo::~AcceleratorInfo() = default;
FeaturePromoSpecification::AcceleratorInfo::AcceleratorInfo(ValueType value)
    : value_(value) {}
FeaturePromoSpecification::AcceleratorInfo&
FeaturePromoSpecification::AcceleratorInfo::operator=(
    const AcceleratorInfo& other) = default;

FeaturePromoSpecification::AcceleratorInfo&
FeaturePromoSpecification::AcceleratorInfo::operator=(ValueType value) {
  value_ = value;
  return *this;
}

FeaturePromoSpecification::AcceleratorInfo::operator bool() const {
  return absl::holds_alternative<ui::Accelerator>(value_) ||
         absl::get<int>(value_);
}

ui::Accelerator FeaturePromoSpecification::AcceleratorInfo::GetAccelerator(
    const ui::AcceleratorProvider* provider) const {
  if (absl::holds_alternative<ui::Accelerator>(value_))
    return absl::get<ui::Accelerator>(value_);

  const int command_id = absl::get<int>(value_);
  DCHECK_GT(command_id, 0);

  ui::Accelerator result;
  DCHECK(provider->GetAcceleratorForCommandId(command_id, &result));
  return result;
}

FeaturePromoSpecification::DemoPageInfo::DemoPageInfo(
    std::string display_title_,
    std::string display_description_,
    base::RepeatingClosure setup_for_feature_promo_callback_)
    : display_title(display_title_),
      display_description(display_description_),
      setup_for_feature_promo_callback(setup_for_feature_promo_callback_) {}

FeaturePromoSpecification::DemoPageInfo::DemoPageInfo(
    const DemoPageInfo& other) = default;

FeaturePromoSpecification::DemoPageInfo::~DemoPageInfo() = default;

FeaturePromoSpecification::DemoPageInfo&
FeaturePromoSpecification::DemoPageInfo::operator=(const DemoPageInfo& other) =
    default;

// static
constexpr HelpBubbleArrow FeaturePromoSpecification::kDefaultBubbleArrow;

FeaturePromoSpecification::FeaturePromoSpecification() = default;

FeaturePromoSpecification::FeaturePromoSpecification(
    FeaturePromoSpecification&& other) = default;

FeaturePromoSpecification::FeaturePromoSpecification(
    const base::Feature* feature,
    PromoType promo_type,
    ui::ElementIdentifier anchor_element_id,
    int bubble_body_string_id)
    : feature_(feature),
      promo_type_(promo_type),
      anchor_element_id_(anchor_element_id),
      bubble_body_string_id_(bubble_body_string_id),
      demo_page_info_(DemoPageInfo(feature ? feature->name : std::string())),
      custom_action_dismiss_string_id_(IDS_PROMO_DISMISS_BUTTON) {
  DCHECK_NE(promo_type, PromoType::kUnspecified);
  DCHECK(bubble_body_string_id_);
}

FeaturePromoSpecification::~FeaturePromoSpecification() = default;

FeaturePromoSpecification& FeaturePromoSpecification::operator=(
    FeaturePromoSpecification&& other) = default;

std::u16string FeaturePromoSpecification::FormatString(
    int string_id,
    const FormatParameters& format_params) {
  if (!string_id) {
    CHECK(absl::holds_alternative<NoSubstitution>(format_params));
    return std::u16string();
  }
  if (absl::holds_alternative<NoSubstitution>(format_params)) {
    return l10n_util::GetStringUTF16(string_id);
  }
  if (const auto* substitutions =
          absl::get_if<StringSubstitutions>(&format_params)) {
    return l10n_util::GetStringFUTF16(string_id, *substitutions, nullptr);
  }
  if (const std::u16string* str =
          absl::get_if<std::u16string>(&format_params)) {
    return l10n_util::GetStringFUTF16(string_id, *str);
  }
  int number = absl::get<int>(format_params);
  return l10n_util::GetPluralStringFUTF16(string_id, number);
}

// static
FeaturePromoSpecification FeaturePromoSpecification::CreateForToastPromo(
    const base::Feature& feature,
    ui::ElementIdentifier anchor_element_id,
    int body_text_string_id,
    int accessible_text_string_id,
    AcceleratorInfo accessible_accelerator) {
  FeaturePromoSpecification spec(&feature, PromoType::kToast, anchor_element_id,
                                 body_text_string_id);
  CHECK_NE(body_text_string_id, accessible_text_string_id)
      << "Because toasts are hard to notice and time out quickly, screen "
         "reader text associated with toasts should differ from the bubble "
         "text and either provide the accelerator to access the highlighted "
         "entry point for your feature, or at the very least provide a "
         "separate description of the screen element appropriate for keyboard "
         "and low-vision users.";
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
FeaturePromoSpecification FeaturePromoSpecification::CreateForLegacyPromo(
    const base::Feature* feature,
    ui::ElementIdentifier anchor_element_id,
    int body_text_string_id) {
  return FeaturePromoSpecification(feature, PromoType::kLegacy,
                                   anchor_element_id, body_text_string_id);
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
  bubble_arrow_ = bubble_arrow;
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetPromoSubtype(
    PromoSubtype promo_subtype) {
  CHECK(promo_type_ != PromoType::kUnspecified);
  CHECK(promo_type_ != PromoType::kSnooze)
      << "Basic snooze is not compatible with other promo subtypes.";
  if (promo_subtype == PromoSubtype::kLegalNotice) {
    CHECK(feature_);
    CHECK(IsAllowedLegalNotice(*feature_));
  }
  promo_subtype_ = promo_subtype;
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetAnchorElementFilter(
    AnchorElementFilter anchor_element_filter) {
  anchor_element_filter_ = std::move(anchor_element_filter);
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetInAnyContext(
    bool in_any_context) {
  in_any_context_ = in_any_context;
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetDemoPageInfo(
    DemoPageInfo demo_page_info) {
  demo_page_info_ = std::move(demo_page_info);
  return *this;
}

FeaturePromoSpecification& FeaturePromoSpecification::SetCustomActionIsDefault(
    bool custom_action_is_default) {
  DCHECK(!custom_action_callback_.is_null());
  custom_action_is_default_ = custom_action_is_default;
  return *this;
}

FeaturePromoSpecification&
FeaturePromoSpecification::SetCustomActionDismissText(
    int custom_action_dismiss_string_id) {
  DCHECK(promo_type_ == PromoType::kCustomAction);
  custom_action_dismiss_string_id_ = custom_action_dismiss_string_id;
  return *this;
}

ui::TrackedElement* FeaturePromoSpecification::GetAnchorElement(
    ui::ElementContext context) const {
  auto* const element_tracker = ui::ElementTracker::GetElementTracker();
  if (anchor_element_filter_) {
    return anchor_element_filter_.Run(
        in_any_context_ ? element_tracker->GetAllMatchingElementsInAnyContext(
                              anchor_element_id_)
                        : element_tracker->GetAllMatchingElements(
                              anchor_element_id_, context));
  } else {
    return in_any_context_
               ? element_tracker->GetElementInAnyContext(anchor_element_id_)
               : element_tracker->GetFirstMatchingElement(anchor_element_id_,
                                                          context);
  }
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
    case FeaturePromoSpecification::PromoSubtype::kPerApp:
      oss << "kPerApp";
      break;
    case FeaturePromoSpecification::PromoSubtype::kLegalNotice:
      oss << "kLegalNotice";
      break;
  }
  return oss;
}

}  // namespace user_education
