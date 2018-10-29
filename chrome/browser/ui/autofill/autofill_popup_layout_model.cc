// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"

#include <algorithm>

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

#if !defined(OS_ANDROID)
#include "chrome/app/vector_icons/vector_icons.h"
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

namespace autofill {

namespace {

// The vertical height of each row in pixels.
const size_t kRowHeight = 24;

// The vertical height of a separator in pixels.
const size_t kSeparatorHeight = 1;

#if !defined(OS_ANDROID)
// Size difference between the normal font and the smaller font, in pixels.
const int kSmallerFontSizeDelta = -1;
#endif

// Used in the IDS_ space as a placeholder for resources that don't exist.
constexpr int kResourceNotFoundId = 0;

const struct {
  const char* name;
  int icon_id;
  int accessible_string_id;
} kDataResources[] = {
    {autofill::kAmericanExpressCard, IDR_AUTOFILL_CC_AMEX,
     IDS_AUTOFILL_CC_AMEX},
    {autofill::kDinersCard, IDR_AUTOFILL_CC_DINERS, IDS_AUTOFILL_CC_DINERS},
    {autofill::kDiscoverCard, IDR_AUTOFILL_CC_DISCOVER,
     IDS_AUTOFILL_CC_DISCOVER},
    {autofill::kEloCard, IDR_AUTOFILL_CC_ELO, IDS_AUTOFILL_CC_ELO},
    {autofill::kGenericCard, IDR_AUTOFILL_CC_GENERIC, kResourceNotFoundId},
    {autofill::kJCBCard, IDR_AUTOFILL_CC_JCB, IDS_AUTOFILL_CC_JCB},
    {autofill::kMasterCard, IDR_AUTOFILL_CC_MASTERCARD,
     IDS_AUTOFILL_CC_MASTERCARD},
    {autofill::kMirCard, IDR_AUTOFILL_CC_MIR, IDS_AUTOFILL_CC_MIR},
    {autofill::kUnionPay, IDR_AUTOFILL_CC_UNIONPAY, IDS_AUTOFILL_CC_UNION_PAY},
    {autofill::kVisaCard, IDR_AUTOFILL_CC_VISA, IDS_AUTOFILL_CC_VISA},
    {"googlePay", IDR_AUTOFILL_GOOGLE_PAY, kResourceNotFoundId},
#if defined(OS_ANDROID)
    {"httpWarning", IDR_AUTOFILL_HTTP_WARNING, kResourceNotFoundId},
    {"httpsInvalid", IDR_AUTOFILL_HTTPS_INVALID_WARNING, kResourceNotFoundId},
    {"scanCreditCardIcon", IDR_AUTOFILL_CC_SCAN_NEW, kResourceNotFoundId},
    {"settings", IDR_AUTOFILL_SETTINGS, kResourceNotFoundId},
    {"create", IDR_AUTOFILL_CREATE, kResourceNotFoundId},
#endif
};

int GetRowHeightFromId(int identifier) {
  if (identifier == POPUP_ITEM_ID_SEPARATOR)
    return kSeparatorHeight;

  return kRowHeight;
}

}  // namespace

AutofillPopupLayoutModel::AutofillPopupLayoutModel(
    AutofillPopupViewDelegate* delegate, bool is_credit_card_popup)
    : delegate_(delegate), is_credit_card_popup_(is_credit_card_popup) {
#if !defined(OS_ANDROID)
  smaller_font_list_ =
      normal_font_list_.DeriveWithSizeDelta(kSmallerFontSizeDelta);
  bold_font_list_ = normal_font_list_.DeriveWithWeight(gfx::Font::Weight::BOLD);
  view_common_ = std::make_unique<PopupViewCommon>();
#endif
}

AutofillPopupLayoutModel::~AutofillPopupLayoutModel() {}

#if !defined(OS_ANDROID)
int AutofillPopupLayoutModel::GetDesiredPopupHeight() const {
  std::vector<autofill::Suggestion> suggestions = delegate_->GetSuggestions();
  int popup_height = 2 * kPopupBorderThickness;

  for (size_t i = 0; i < suggestions.size(); ++i) {
    popup_height += GetRowHeightFromId(suggestions[i].frontend_id);
  }

  return popup_height;
}

int AutofillPopupLayoutModel::GetDesiredPopupWidth() const {
  std::vector<autofill::Suggestion> suggestions = delegate_->GetSuggestions();

  int popup_width = RoundedElementBounds().width();

  for (size_t i = 0; i < suggestions.size(); ++i) {
    int label_size = delegate_->GetElidedLabelWidthForRow(i);
    int row_size = delegate_->GetElidedValueWidthForRow(i) + label_size +
                   RowWidthWithoutText(i, /* has_subtext= */ label_size > 0);

    popup_width = std::max(popup_width, row_size);
  }

  return popup_width;
}

int AutofillPopupLayoutModel::RowWidthWithoutText(int row,
                                                  bool has_subtext) const {
  std::vector<autofill::Suggestion> suggestions = delegate_->GetSuggestions();
  int row_size = 2 * (kEndPadding + kPopupBorderThickness);
  if (has_subtext)
    row_size += kNamePadding;

  // Add the Autofill icon size, if required.
  const base::string16& icon = suggestions[row].icon;
  if (!icon.empty()) {
    row_size += GetIconImage(row).width() + kIconPadding;
  }
  return row_size;
}

int AutofillPopupLayoutModel::GetAvailableWidthForRow(int row,
                                                      bool has_subtext) const {
  return popup_bounds_.width() - RowWidthWithoutText(row, has_subtext);
}

void AutofillPopupLayoutModel::UpdatePopupBounds() {
  int popup_width = GetDesiredPopupWidth();
  int popup_height = GetDesiredPopupHeight();

  popup_bounds_ = view_common_->CalculatePopupBounds(
      popup_width, popup_height, RoundedElementBounds(),
      delegate_->container_view(), delegate_->IsRTL());
}

const gfx::FontList& AutofillPopupLayoutModel::GetValueFontListForRow(
    size_t index) const {
  std::vector<autofill::Suggestion> suggestions = delegate_->GetSuggestions();

  // Autofill values have positive |frontend_id|.
  if (suggestions[index].frontend_id > 0)
    return bold_font_list_;

  // All other message types are defined here.
  PopupItemId id = static_cast<PopupItemId>(suggestions[index].frontend_id);
  switch (id) {
    case POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE:
    case POPUP_ITEM_ID_CLEAR_FORM:
    case POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO:
    case POPUP_ITEM_ID_AUTOFILL_OPTIONS:
    case POPUP_ITEM_ID_CREATE_HINT:
    case POPUP_ITEM_ID_SCAN_CREDIT_CARD:
    case POPUP_ITEM_ID_SEPARATOR:
    case POPUP_ITEM_ID_TITLE:
    case POPUP_ITEM_ID_PASSWORD_ENTRY:
    case POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY:
    case POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY:
    case POPUP_ITEM_ID_GOOGLE_PAY_BRANDING:
      return normal_font_list_;
    case POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY:
    case POPUP_ITEM_ID_DATALIST_ENTRY:
    case POPUP_ITEM_ID_USERNAME_ENTRY:
      return bold_font_list_;
  }
  NOTREACHED();
  return normal_font_list_;
}

const gfx::FontList& AutofillPopupLayoutModel::GetLabelFontListForRow(
    size_t index) const {
  return smaller_font_list_;
}

ui::NativeTheme::ColorId AutofillPopupLayoutModel::GetValueFontColorIDForRow(
    size_t index) const {
  std::vector<autofill::Suggestion> suggestions = delegate_->GetSuggestions();
  switch (suggestions[index].frontend_id) {
    case POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE:
      return ui::NativeTheme::kColorId_ResultsTableDimmedText;
    default:
      return ui::NativeTheme::kColorId_ResultsTableNormalText;
  }
}

gfx::ImageSkia AutofillPopupLayoutModel::GetIconImage(size_t index) const {
  std::vector<autofill::Suggestion> suggestions = delegate_->GetSuggestions();
  if (!suggestions[index].custom_icon.IsEmpty())
    return suggestions[index].custom_icon.AsImageSkia();

  const base::string16& icon_str = suggestions[index].icon;
  if (icon_str.empty())
    return gfx::ImageSkia();

  constexpr int kIconSize = 16;

  // For http warning message, get icon images from VectorIcon, which is the
  // same as security indicator icons in location bar.
  if (icon_str == base::ASCIIToUTF16("httpWarning")) {
    return gfx::CreateVectorIcon(omnibox::kHttpIcon, kIconSize,
                                 gfx::kChromeIconGrey);
  }
  if (icon_str == base::ASCIIToUTF16("httpsInvalid")) {
    return gfx::CreateVectorIcon(omnibox::kHttpsInvalidIcon, kIconSize,
                                 gfx::kGoogleRed700);
  }
  if (icon_str == base::ASCIIToUTF16("keyIcon"))
    return gfx::CreateVectorIcon(kKeyIcon, kIconSize, gfx::kChromeIconGrey);
  if (icon_str == base::ASCIIToUTF16("globeIcon"))
    return gfx::CreateVectorIcon(kGlobeIcon, kIconSize, gfx::kChromeIconGrey);

  // For other suggestion entries, get icon from PNG files.
  int icon_id = GetIconResourceID(icon_str);
  DCHECK_NE(kResourceNotFoundId, icon_id);
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(icon_id);
}
#endif  // !defined(OS_ANDROID)

int AutofillPopupLayoutModel::LineFromY(int y) const {
  std::vector<autofill::Suggestion> suggestions = delegate_->GetSuggestions();
  int current_height = kPopupBorderThickness;

  for (size_t i = 0; i < suggestions.size(); ++i) {
    current_height += GetRowHeightFromId(suggestions[i].frontend_id);

    if (y <= current_height)
      return i;
  }

  // The y value goes beyond the popup so stop the selection at the last line.
  return suggestions.size() - 1;
}

gfx::Rect AutofillPopupLayoutModel::GetRowBounds(size_t index) const {
  std::vector<autofill::Suggestion> suggestions = delegate_->GetSuggestions();

  int top = kPopupBorderThickness;
  for (size_t i = 0; i < index; ++i) {
    top += GetRowHeightFromId(suggestions[i].frontend_id);
  }

  return gfx::Rect(kPopupBorderThickness, top,
                   popup_bounds_.width() - 2 * kPopupBorderThickness,
                   GetRowHeightFromId(suggestions[index].frontend_id));
}

int AutofillPopupLayoutModel::GetIconResourceID(
    const base::string16& resource_name) const {
  int result = kResourceNotFoundId;
  for (size_t i = 0; i < base::size(kDataResources); ++i) {
    if (resource_name == base::ASCIIToUTF16(kDataResources[i].name)) {
      result = kDataResources[i].icon_id;
      break;
    }
  }

  return result;
}

int AutofillPopupLayoutModel::GetIconAccessibleNameResourceId(
    const base::string16& resource_name) const {
  for (size_t i = 0; i < base::size(kDataResources); ++i) {
    // TODO(crbug.com/850597): Remove UTF conversion once AutofillSuggestion
    // no longer stores the resource name as a string16.
    if (resource_name == base::ASCIIToUTF16(kDataResources[i].name))
      return kDataResources[i].accessible_string_id;
  }
  return kResourceNotFoundId;
}

void AutofillPopupLayoutModel::SetUpForTesting(
    std::unique_ptr<PopupViewCommon> view_common) {
  view_common_ = std::move(view_common);
}

const gfx::Rect AutofillPopupLayoutModel::RoundedElementBounds() const {
  return gfx::ToEnclosingRect(delegate_->element_bounds());
}

}  // namespace autofill
