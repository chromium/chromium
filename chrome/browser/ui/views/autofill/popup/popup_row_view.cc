// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/autofill/autofill_popup_controller_utils.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/chrome_typography_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/font.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"

using autofill::PopupItemId;
using views::BubbleBorder;

namespace {

// The duration for which clicks on the just-shown Autofill popup should be
// ignored.
constexpr base::TimeDelta kIgnoreEarlyClicksOnPopupDuration =
    base::Milliseconds(500);

// By spec, dropdowns should always have a width which is a multiple of 12.
constexpr int kAutofillPopupWidthMultiple = 12;

// Max width for the username and masked password.
constexpr int kAutofillPopupUsernameMaxWidth = 272;
constexpr int kAutofillPopupPasswordMaxWidth = 108;

// Max width for address profile suggestion text.
constexpr int kAutofillPopupAddressProfileMaxWidth = 192;
// Max width for address credit card suggestion text.
constexpr int kAutofillPopupCreditCardMaxWidth =
    kAutofillPopupWidthMultiple * 16;

// The additional height of the row in case it has two lines of text.
constexpr int kAutofillPopupAdditionalDoubleRowHeight = 22;

// The additional padding of the row in case it has three lines of text.
constexpr int kAutofillPopupAdditionalPadding = 16;

// Vertical spacing between labels in one row.
constexpr int kAdjacentLabelsVerticalSpacing = 2;

// The default icon size used in the suggestion drop down.
constexpr int kIconSize = 16;

// The icon size used in the suggestion dropdown for displaying the Google
// Password Manager icon in the Manager Passwords entry.
constexpr int kGooglePasswordManagerIconSize = 20;

// Popup items that use a leading icon instead of a trailing one.
constexpr PopupItemId kItemTypesUsingLeadingIcons[] = {
    PopupItemId::POPUP_ITEM_ID_CLEAR_FORM,
    PopupItemId::POPUP_ITEM_ID_SHOW_ACCOUNT_CARDS,
    PopupItemId::POPUP_ITEM_ID_AUTOFILL_OPTIONS,
    PopupItemId::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY,
    PopupItemId::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_EMPTY,
    PopupItemId::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN,
    PopupItemId::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_RE_SIGNIN,
    PopupItemId::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE};

// Builds a column set for |layout| used in the autofill dropdown.
void BuildColumnSet(views::TableLayoutView* layout_view) {
  const int column_divider = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);

  layout_view
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kStretch,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, column_divider)
      .AddColumn(views::LayoutAlignment::kStart,
                 views::LayoutAlignment::kStretch,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
}

std::unique_ptr<views::ImageView> ImageViewFromImageSkia(
    const gfx::ImageSkia& image_skia) {
  if (image_skia.isNull()) {
    return nullptr;
  }
  auto image_view = std::make_unique<views::ImageView>();
  image_view->SetImage(image_skia);
  return image_view;
}

std::unique_ptr<views::ImageView> ImageViewFromVectorIcon(
    const gfx::VectorIcon& vector_icon,
    int icon_size) {
  return std::make_unique<views::ImageView>(
      ui::ImageModel::FromVectorIcon(vector_icon, ui::kColorIcon, icon_size));
}

std::unique_ptr<views::ImageView> ImageViewFromVectorIcon(
    const gfx::VectorIcon& vector_icon) {
  return ImageViewFromVectorIcon(vector_icon, kIconSize);
}

std::unique_ptr<views::ImageView> GetIconImageViewByName(
    const std::string& icon_str) {
  if (icon_str.empty()) {
    return nullptr;
  }

  // For http warning message, get icon images from VectorIcon, which is the
  // same as security indicator icons in location bar.
  if (icon_str == "httpWarning") {
    return ImageViewFromVectorIcon(omnibox::kHttpIcon);
  }

  if (icon_str == "httpsInvalid") {
    return std::make_unique<views::ImageView>(
        ui::ImageModel::FromVectorIcon(vector_icons::kNotSecureWarningIcon,
                                       ui::kColorAlertHighSeverity, kIconSize));
  }

  if (icon_str == "keyIcon") {
    return ImageViewFromVectorIcon(kKeyIcon);
  }

  if (icon_str == "clearIcon") {
    return ImageViewFromVectorIcon(kBackspaceIcon);
  }

  if (icon_str == "globeIcon") {
    return ImageViewFromVectorIcon(kGlobeIcon);
  }

  if (icon_str == "accountIcon") {
    return ImageViewFromVectorIcon(kAccountCircleIcon);
  }

  if (icon_str == "settingsIcon") {
    return ImageViewFromVectorIcon(kMonoColorProductIcon);
  }

  if (icon_str == "empty") {
    return ImageViewFromVectorIcon(omnibox::kHttpIcon);
  }

  if (icon_str == "device") {
    return ImageViewFromVectorIcon(kLaptopAndSmartphoneIcon);
  }

  if (icon_str == "google") {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    return ImageViewFromImageSkia(gfx::CreateVectorIcon(
        vector_icons::kGoogleGLogoIcon, kIconSize, gfx::kPlaceholderColor));
#else
    return nullptr;
#endif
  }

  if (icon_str == "googlePasswordManager") {
    return ImageViewFromVectorIcon(GooglePasswordManagerVectorIcon(),
                                   kGooglePasswordManagerIconSize);
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (icon_str == "googlePay" || icon_str == "googlePayDark") {
    return nullptr;
  }
#endif
  // For other suggestion entries, get icon from PNG files.
  int icon_id = autofill::GetIconResourceID(icon_str);
  DCHECK_NE(icon_id, 0);
  return ImageViewFromImageSkia(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(icon_id));
}

std::unique_ptr<views::ImageView> GetIconImageView(
    const autofill::Suggestion& suggestion) {
  if (!suggestion.custom_icon.IsEmpty()) {
    return ImageViewFromImageSkia(suggestion.custom_icon.AsImageSkia());
  }

  return GetIconImageViewByName(suggestion.icon);
}

std::unique_ptr<views::ImageView> GetTrailingIconImageView(
    const autofill::Suggestion& suggestion) {
  return GetIconImageViewByName(suggestion.trailing_icon);
}

// Creates a label with a specific context and style.
std::unique_ptr<views::Label> CreateLabelWithStyleAndContext(
    const std::u16string& text,
    int text_context,
    int text_style) {
  auto label = std::make_unique<views::Label>(text, text_context, text_style);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  return label;
}

// Returns the name of the network for payment method icons, empty string
// otherwise.
std::u16string GetIconAccessibleName(const std::string& icon_text) {
  // Networks for which icons are currently shown.
  if (icon_text == autofill::kAmericanExpressCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_AMEX);
  }
  if (icon_text == autofill::kDinersCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_DINERS);
  }
  if (icon_text == autofill::kDiscoverCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_DISCOVER);
  }
  if (icon_text == autofill::kEloCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_ELO);
  }
  if (icon_text == autofill::kJCBCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_JCB);
  }
  if (icon_text == autofill::kMasterCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_MASTERCARD);
  }
  if (icon_text == autofill::kMirCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_MIR);
  }
  if (icon_text == autofill::kTroyCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_TROY);
  }
  if (icon_text == autofill::kUnionPay) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_UNION_PAY);
  }
  if (icon_text == autofill::kVisaCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_VISA);
  }
  // Other networks.
  if (icon_text == autofill::kGenericCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_GENERIC);
  }
  return std::u16string();
}

}  // namespace

namespace autofill {

/************** PopupRowView **************/

PopupRowView::~PopupRowView() = default;

PopupRowView::PopupRowView(PopupViewViews& popup_view,
                           int line_number,
                           int frontend_id)
    : popup_view_(popup_view),
      line_number_(line_number),
      frontend_id_(frontend_id) {
  // TODO(crbug.com/1411172): Remove once the responsibility to track selected
  // state is moved to individual cells.
  SetNotifyEnterExitOnChild(true);
}

void PopupRowView::SetSelected(bool selected) {
  if (selected == selected_) {
    return;
  }

  selected_ = selected;
  if (selected) {
    popup_view().NotifyAXSelection(this);
  }
  RefreshStyle();
  OnPropertyChanged(&selected_, views::kPropertyEffectsNone);
}

void PopupRowView::MaybeShowIphPromo() {
  std::string feature_name = popup_view()
                                 .controller()
                                 ->GetSuggestionAt(GetLineNumber())
                                 .feature_for_iph;
  if (feature_name.empty()) {
    return;
  }

  if (feature_name == "IPH_AutofillVirtualCardSuggestion") {
    SetProperty(views::kElementIdentifierKey,
                kAutofillCreditCardSuggestionEntryElementId);
    Browser* browser = popup_view().browser();
    DCHECK(browser);
    browser->window()->MaybeShowFeaturePromo(
        feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);
  }
}

bool PopupRowView::OnMouseDragged(const ui::MouseEvent& event) {
  return true;
}

bool PopupRowView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

ui::ColorId PopupRowView::GetBackgroundColorId() const {
  return GetSelected() ? ui::kColorDropdownBackgroundSelected
                       : ui::kColorDropdownBackground;
}

bool PopupRowView::HandleAccessibleAction(const ui::AXActionData& action_data) {
  base::WeakPtr<AutofillPopupController> controller = popup_view().controller();
  if (controller && action_data.action == ax::mojom::Action::kFocus) {
    controller->SetSelectedLine(line_number_);
  }
  return View::HandleAccessibleAction(action_data);
}

void PopupRowView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // Options are selectable.
  node_data->role = ax::mojom::Role::kListBoxOption;
  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                              GetSelected());

  // It is possible for the screen reader to request the a11y role of an
  // PopupRowView one final time when the Autofill popup gets hidden.
  // At this point the controller is already invalid. Therefore we skip all
  // steps that require a controller.
  base::WeakPtr<AutofillPopupController> controller = popup_view().controller();
  if (!controller) {
    return;
  }

  node_data->SetNameChecked(GetVoiceOverString());

  // Compute set size and position in set, by checking the frontend_id of each
  // row, summing the number of interactive rows, and subtracting the number
  // of separators found before this row from its |pos_in_set|.
  int set_size = 0;
  int pos_in_set = GetLineNumber() + 1;
  for (int i = 0; i < controller->GetLineCount(); ++i) {
    if (controller->GetSuggestionAt(i).frontend_id ==
        autofill::POPUP_ITEM_ID_SEPARATOR) {
      if (i < GetLineNumber()) {
        --pos_in_set;
      }
    } else {
      ++set_size;
    }
  }
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize, set_size);
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, pos_in_set);
}

void PopupRowView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  mouse_observed_outside_item_bounds_ |= !IsMouseInsideItemBounds();
}

void PopupRowView::OnThemeChanged() {
  views::View::OnThemeChanged();
  RefreshStyle();
}

void PopupRowView::OnMouseEntered(const ui::MouseEvent& event) {
  // OnMouseEntered() does not imply that the mouse had been outside of the
  // item's bounds before: OnMouseEntered() also fires if the mouse moves just a
  // little bit on the item. We don't want to show a preview in such a case.
  if (!mouse_observed_outside_item_bounds_) {
    return;
  }

  base::WeakPtr<AutofillPopupController> controller = popup_view().controller();
  if (controller) {
    controller->SetSelectedLine(GetLineNumber());
  }
}

void PopupRowView::OnMouseExited(const ui::MouseEvent& event) {
  // OnMouseExited() does not imply that the mouse has left the item's screen
  // bounds: OnMouseExited() also fires (on Windows, at least) even when another
  // popup overlays this item (crbug.com/1287364).
  mouse_observed_outside_item_bounds_ |= !IsMouseInsideItemBounds();

  base::WeakPtr<AutofillPopupController> controller = popup_view().controller();
  if (controller) {
    controller->SelectionCleared();
  }
}

void PopupRowView::OnMouseReleased(const ui::MouseEvent& event) {
  // Ignore mouse clicks unless the user made the explicit choice to selected
  // the current item.
  if (!mouse_observed_outside_item_bounds_) {
    return;
  }

  // Ignore clicks immediately after the popup was shown. This is to prevent
  // users accidentally accepting suggestions (crbug.com/1279268).
  if (popup_view().time_delta_since_popup_shown() <=
      kIgnoreEarlyClicksOnPopupDuration) {
    return;
  }

  base::WeakPtr<AutofillPopupController> controller = popup_view().controller();
  if (controller && event.IsOnlyLeftMouseButton() &&
      HitTestPoint(event.location())) {
    controller->AcceptSuggestion(GetLineNumber());
  }
}

void PopupRowView::OnGestureEvent(ui::GestureEvent* event) {
  base::WeakPtr<AutofillPopupController> controller = popup_view().controller();
  if (!controller) {
    return;
  }
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      controller->SetSelectedLine(GetLineNumber());
      break;
    case ui::ET_GESTURE_TAP:
      controller->AcceptSuggestion(GetLineNumber());
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_GESTURE_END:
      controller->SelectionCleared();
      break;
    default:
      return;
  }
}

void PopupRowView::CreateContent() {
  base::WeakPtr<AutofillPopupController> controller = popup_view().controller();

  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, PopupBaseView::GetHorizontalMargin())));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  std::vector<Suggestion> suggestions = controller->GetSuggestions();

  std::unique_ptr<views::ImageView> icon =
      GetIconImageView(suggestions[GetLineNumber()]);
  if (icon) {
    AddChildView(std::move(icon));
    AddSpacerWithSize(PopupBaseView::GetHorizontalPadding(),
                      /*resize=*/false, layout_manager);
  }

  std::unique_ptr<views::View> main_text_label = CreateMainTextView();
  std::unique_ptr<views::View> minor_text_label = CreateMinorTextView();
  std::vector<std::unique_ptr<views::View>> subtext_labels =
      CreateSubtextViews();
  std::unique_ptr<views::View> description_label = CreateDescriptionView();

  auto all_labels = std::make_unique<views::TableLayoutView>();
  BuildColumnSet(all_labels.get());
  all_labels->AddRows(1, 0);

  // Create the first line text view.
  if (minor_text_label) {
    auto first_line_container = std::make_unique<views::View>();
    first_line_container
        ->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(views::LayoutAlignment::kStart)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetIgnoreDefaultMainAxisMargins(true)
        .SetCollapseMargins(true)
        .SetDefault(
            views::kMarginsKey,
            gfx::Insets::VH(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                                   DISTANCE_RELATED_LABEL_HORIZONTAL_LIST)));

    first_line_container->AddChildView(std::move(main_text_label));
    first_line_container->AddChildView(std::move(minor_text_label));
    all_labels->AddChildView(std::move(first_line_container));
  } else {
    all_labels->AddChildView(std::move(main_text_label));
  }

  if (description_label) {
    all_labels->AddChildView(std::move(description_label));
  } else {
    all_labels->AddChildView(std::make_unique<views::View>());
  }

  UpdateLayoutSize(layout_manager, subtext_labels.size());
  for (std::unique_ptr<views::View>& subtext_label : subtext_labels) {
    all_labels->AddPaddingRow(0, kAdjacentLabelsVerticalSpacing).AddRows(1, 0);
    all_labels->AddChildView(std::move(subtext_label));
    all_labels->AddChildView(std::make_unique<views::View>());
  }

  AddChildView(std::move(all_labels));
  std::unique_ptr<views::ImageView> trailing_icon =
      GetTrailingIconImageView(suggestions[GetLineNumber()]);
  if (trailing_icon) {
    AddSpacerWithSize(PopupBaseView::GetHorizontalPadding(),
                      /*resize=*/true, layout_manager);
    AddChildView(std::move(trailing_icon));
  }
}

void PopupRowView::RefreshStyle() {
  SetBackground(views::CreateThemedSolidBackground(GetBackgroundColorId()));

  // Set style for each label in this view depending on current state since the
  // style isn't automatically adjusted after creation of the label.
  for (views::Label* label : inner_labels_) {
    label->SetAutoColorReadabilityEnabled(false);
    label->SetBackgroundColorId(GetBackgroundColorId());

    if (!label->GetEnabled()) {
      label->SetEnabledColor(views::style::GetColor(
          *this, label->GetTextContext(), views::style::STYLE_DISABLED));
      continue;
    }

    // If the current suggestion is selected, retrieve a color that corresponds
    // to the SELECTED style. Otherwise, use the color that corresponds to the
    // actual style of the label.
    label->SetEnabledColor(views::style::GetColor(
        *this, label->GetTextContext(),
        GetSelected() ? views::style::STYLE_SELECTED : label->GetTextStyle()));
  }
  SchedulePaint();
}

std::unique_ptr<views::Label> PopupRowView::CreateMainTextView() {
  const Suggestion::Text& main_text =
      popup_view().controller()->GetSuggestionAt(GetLineNumber()).main_text;
  if (!main_text.is_primary) {
    std::unique_ptr<views::Label> label = CreateLabelWithStyleAndContext(
        main_text.value, views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_PRIMARY);
    KeepLabel(label.get());
    return label;
  }

  std::unique_ptr<views::Label> label = CreateLabelWithStyleAndContext(
      popup_view().controller()->GetSuggestionMainTextAt(GetLineNumber()),
      views::style::CONTEXT_DIALOG_BODY_TEXT, GetPrimaryTextStyle());

  const gfx::Font::Weight font_weight = GetPrimaryTextWeight();
  if (font_weight != label->font_list().GetFontWeight()) {
    label->SetFontList(label->font_list().DeriveWithWeight(font_weight));
  }

  KeepLabel(label.get());
  return label;
}

std::unique_ptr<views::Label> PopupRowView::CreateMinorTextView() {
  std::u16string text =
      popup_view().controller()->GetSuggestionMinorTextAt(GetLineNumber());
  if (text.empty()) {
    return nullptr;
  }

  std::unique_ptr<views::Label> label = CreateLabelWithStyleAndContext(
      text, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  KeepLabel(label.get());
  return label;
}

std::unique_ptr<views::View> PopupRowView::CreateDescriptionView() {
  return nullptr;
}

std::vector<std::unique_ptr<views::View>> PopupRowView::CreateSubtextViews() {
  return {};
}

void PopupRowView::UpdateLayoutSize(views::BoxLayout* layout_manager,
                                    int64_t num_subtexts) {
  const int kStandardRowHeight =
      views::MenuConfig::instance().touchable_menu_height;
  if (num_subtexts == 0) {
    layout_manager->set_minimum_cross_axis_size(kStandardRowHeight);
  } else {
    layout_manager->set_minimum_cross_axis_size(
        kStandardRowHeight + kAutofillPopupAdditionalDoubleRowHeight);
  }

  // In the case that there are three rows in total, adding extra padding to
  // avoid cramming.
  if (num_subtexts == 2) {
    layout_manager->set_inside_border_insets(gfx::Insets::TLBR(
        kAutofillPopupAdditionalPadding, PopupBaseView::GetHorizontalMargin(),
        kAutofillPopupAdditionalPadding, PopupBaseView::GetHorizontalMargin()));
  }
}

void PopupRowView::AddSpacerWithSize(int spacer_width,
                                     bool resize,
                                     views::BoxLayout* layout) {
  auto spacer = std::make_unique<views::View>();
  spacer->SetPreferredSize(gfx::Size(spacer_width, 1));
  layout->SetFlexForView(AddChildView(std::move(spacer)),
                         /*flex=*/resize ? 1 : 0,
                         /*use_min_size=*/true);
}

std::u16string PopupRowView::GetVoiceOverString() {
  base::WeakPtr<AutofillPopupController> controller = popup_view().controller();

  auto suggestion = controller->GetSuggestionAt(GetLineNumber());

  if (suggestion.voice_over) {
    return *suggestion.voice_over;
  }

  std::vector<std::u16string> text;
  std::u16string icon_name = GetIconAccessibleName(suggestion.icon);
  if (!icon_name.empty()) {
    text.push_back(icon_name);
  }

  auto main_text = controller->GetSuggestionMainTextAt(GetLineNumber());
  text.push_back(main_text);

  auto minor_text = controller->GetSuggestionMinorTextAt(GetLineNumber());
  if (!minor_text.empty()) {
    text.push_back(minor_text);
  }

  std::vector<std::vector<Suggestion::Text>> labels =
      controller->GetSuggestionLabelsAt(GetLineNumber());
  for (std::vector<Suggestion::Text>& row : labels) {
    for (Suggestion::Text label : row) {
      // |label_text| is not populated for footers or autocomplete entries.
      if (!label.value.empty()) {
        text.push_back(std::move(label.value));
      }
    }
  }

  if (!suggestion.additional_label.empty()) {
    // |additional_label| is only populated in a passwords context.
    text.push_back(suggestion.additional_label);
  }

  return base::JoinString(text, u" ");
}

BEGIN_METADATA(PopupRowView, views::View)
ADD_PROPERTY_METADATA(bool, Selected)
ADD_READONLY_PROPERTY_METADATA(int, LineNumber)
ADD_READONLY_PROPERTY_METADATA(int, FrontendId)
END_METADATA

/************** PopupSuggestionView **************/

// static
std::unique_ptr<PopupSuggestionView> PopupSuggestionView::Create(
    PopupViewViews& popup_view,
    int line_number,
    int frontend_id,
    PopupType popup_type) {
  auto result = base::WrapUnique(new PopupSuggestionView(
      popup_view, line_number, frontend_id, popup_type));
  result->CreateContent();
  return result;
}

int PopupSuggestionView::GetPrimaryTextStyle() {
  return views::style::TextStyle::STYLE_PRIMARY;
}

gfx::Font::Weight PopupSuggestionView::GetPrimaryTextWeight() const {
  return gfx::Font::Weight::NORMAL;
}

PopupSuggestionView::PopupSuggestionView(PopupViewViews& popup_view,
                                         int line_number,
                                         int frontend_id,
                                         PopupType popup_type)
    : PopupRowView(popup_view, line_number, frontend_id),
      popup_type_(popup_type) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

std::unique_ptr<views::Label> PopupSuggestionView::CreateMainTextView() {
  std::unique_ptr<views::Label> label = PopupRowView::CreateMainTextView();
  if (popup_type_ == PopupType::kAddresses) {
    label->SetMaximumWidthSingleLine(kAutofillPopupAddressProfileMaxWidth);
  } else if (popup_type_ == PopupType::kCreditCards &&
             popup_view()
                 .controller()
                 ->GetSuggestionAt(GetLineNumber())
                 .main_text.should_truncate.value()) {
    // should_truncate should only be set to true iff the experiments are
    // enabled.
    DCHECK(base::FeatureList::IsEnabled(
        autofill::features::kAutofillEnableVirtualCardMetadata));
    DCHECK(base::FeatureList::IsEnabled(
        autofill::features::kAutofillEnableCardProductName));
    label->SetMaximumWidthSingleLine(kAutofillPopupCreditCardMaxWidth);
  }
  return label;
}

std::vector<std::unique_ptr<views::View>>
PopupSuggestionView::CreateSubtextViews() {
  std::vector<std::unique_ptr<views::View>> subtext_view;
  for (const std::vector<Suggestion::Text>& label_row :
       popup_view().controller()->GetSuggestionLabelsAt(GetLineNumber())) {
    DCHECK_LE(label_row.size(), 2U);
    DCHECK(!label_row.empty());
    if (base::ranges::all_of(label_row, &std::u16string::empty,
                             &Suggestion::Text::value)) {
      // If a row is empty, do not include any further rows.
      return subtext_view;
    }

    auto label_row_container_view = std::make_unique<views::BoxLayoutView>();
    label_row_container_view->SetOrientation(
        views::BoxLayout::Orientation::kHorizontal);
    label_row_container_view->SetBetweenChildSpacing(
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
    for (const auto& label : label_row) {
      // If a column is empty, do not include any further columns.
      if (label.value.empty()) {
        break;
      }

      auto* label_view =
          label_row_container_view->AddChildView(CreateLabelWithStyleAndContext(
              label.value, ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
              views::style::STYLE_SECONDARY));
      KeepLabel(label_view);
      if (popup_type_ == PopupType::kAddresses) {
        label_view->SetMaximumWidthSingleLine(
            kAutofillPopupAddressProfileMaxWidth);
      } else if (popup_type_ == PopupType::kCreditCards &&
                 label.should_truncate.value()) {
        // should_truncate should only be set to true iff the experiments are
        // enabled.
        DCHECK(base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableVirtualCardMetadata));
        DCHECK(base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableCardProductName));
        label_view->SetMaximumWidthSingleLine(kAutofillPopupCreditCardMaxWidth);
      }
    }
    subtext_view.push_back(std::move(label_row_container_view));
  }

  return subtext_view;
}

BEGIN_METADATA(PopupSuggestionView, PopupRowView)
END_METADATA

/************** PopupPasswordSuggestionView **************/

std::unique_ptr<PopupPasswordSuggestionView>
PopupPasswordSuggestionView::Create(PopupViewViews& popup_view,
                                    int line_number,
                                    int frontend_id) {
  auto result = base::WrapUnique(
      new PopupPasswordSuggestionView(popup_view, line_number, frontend_id));
  result->CreateContent();
  return result;
}

std::unique_ptr<views::Label>
PopupPasswordSuggestionView::CreateMainTextView() {
  std::unique_ptr<views::Label> label =
      PopupSuggestionView::CreateMainTextView();
  label->SetMaximumWidthSingleLine(kAutofillPopupUsernameMaxWidth);
  return label;
}

std::vector<std::unique_ptr<views::View>>
PopupPasswordSuggestionView::CreateSubtextViews() {
  std::unique_ptr<views::Label> label = CreateLabelWithStyleAndContext(
      masked_password_, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  label->SetElideBehavior(gfx::TRUNCATE);
  KeepLabel(label.get());
  label->SetMaximumWidthSingleLine(kAutofillPopupPasswordMaxWidth);

  std::vector<std::unique_ptr<views::View>> labels;
  labels.emplace_back(std::move(label));
  return labels;
}

std::unique_ptr<views::View>
PopupPasswordSuggestionView::CreateDescriptionView() {
  if (origin_.empty()) {
    return nullptr;
  }

  std::unique_ptr<views::Label> label = CreateLabelWithStyleAndContext(
      origin_, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  label->SetElideBehavior(gfx::ELIDE_HEAD);
  KeepLabel(label.get());
  label->SetMaximumWidthSingleLine(kAutofillPopupUsernameMaxWidth);
  return label;
}

gfx::Font::Weight PopupPasswordSuggestionView::GetPrimaryTextWeight() const {
  return gfx::Font::Weight::NORMAL;
}

PopupPasswordSuggestionView::PopupPasswordSuggestionView(
    PopupViewViews& popup_view,
    int line_number,
    int frontend_id)
    : PopupSuggestionView(popup_view,
                          line_number,
                          frontend_id,
                          PopupType::kPasswords) {
  std::vector<std::vector<Suggestion::Text>> labels =
      popup_view.controller()->GetSuggestionLabelsAt(line_number);
  if (!labels.empty()) {
    DCHECK_EQ(labels.size(), 1U);
    DCHECK_EQ(labels[0].size(), 1U);
    origin_ = std::move(labels[0][0].value);
  }

  masked_password_ =
      popup_view.controller()->GetSuggestionAt(line_number).additional_label;
}

BEGIN_METADATA(PopupPasswordSuggestionView, PopupSuggestionView)
END_METADATA

/************** PopupFooterView **************/

// static
std::unique_ptr<PopupFooterView> PopupFooterView::Create(
    PopupViewViews& popup_view,
    int line_number,
    int frontend_id) {
  auto result = base::WrapUnique(
      new PopupFooterView(popup_view, line_number, frontend_id));
  result->CreateContent();
  return result;
}

void PopupFooterView::CreateContent() {
  base::WeakPtr<AutofillPopupController> controller = popup_view().controller();

  SetBorder(views::CreateThemedSolidSidedBorder(gfx::Insets(),
                                                ui::kColorMenuSeparator));

  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::VH(0, PopupBaseView::GetHorizontalMargin())));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  const Suggestion suggestion = controller->GetSuggestions()[GetLineNumber()];
  std::unique_ptr<views::ImageView> icon = GetIconImageView(suggestion);

  const bool use_leading_icon =
      base::Contains(kItemTypesUsingLeadingIcons, GetFrontendId());

  if (suggestion.is_loading) {
    SetEnabled(false);
    AddChildView(std::make_unique<views::Throbber>())->Start();
    AddSpacerWithSize(PopupBaseView::GetHorizontalPadding(),
                      /*resize=*/false, layout_manager);
  } else if (icon && use_leading_icon) {
    AddChildView(std::move(icon));
    AddSpacerWithSize(PopupBaseView::GetHorizontalPadding(),
                      /*resize=*/false, layout_manager);
  }

  layout_manager->set_minimum_cross_axis_size(
      views::MenuConfig::instance().touchable_menu_height);

  auto main_text_label = CreateMainTextView();
  main_text_label->SetEnabled(!suggestion.is_loading);
  AddChildView(std::move(main_text_label));

  AddSpacerWithSize(0, /*resize=*/true, layout_manager);

  if (icon && !use_leading_icon) {
    AddSpacerWithSize(PopupBaseView::GetHorizontalPadding(),
                      /*resize=*/false, layout_manager);
    AddChildView(std::move(icon));
  }

  std::unique_ptr<views::ImageView> trailing_icon =
      GetTrailingIconImageView(suggestion);
  if (trailing_icon) {
    AddSpacerWithSize(PopupBaseView::GetHorizontalPadding(),
                      /*resize=*/true, layout_manager);
    AddChildView(std::move(trailing_icon));
  }
}

int PopupFooterView::GetPrimaryTextStyle() {
  return views::style::STYLE_SECONDARY;
}

gfx::Font::Weight PopupFooterView::GetPrimaryTextWeight() const {
  return gfx::Font::Weight::NORMAL;
}

PopupFooterView::PopupFooterView(PopupViewViews& popup_view,
                                 int line_number,
                                 int frontend_id)
    : PopupRowView(popup_view, line_number, frontend_id) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

BEGIN_METADATA(PopupFooterView, PopupRowView)
END_METADATA

}  // namespace autofill
