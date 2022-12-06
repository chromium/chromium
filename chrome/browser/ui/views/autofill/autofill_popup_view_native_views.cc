// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_view_native_views.h"

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/autofill/autofill_popup_controller_utils.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/autofill/autofill_popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/chrome_typography_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
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
#include "third_party/skia/include/core/SkColor.h"
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
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using autofill::PopupItemId;
using views::BubbleBorder;

namespace {

// The duration for which clicks on the just-shown Autofill popup should be
// ignored.
constexpr base::TimeDelta kIgnoreEarlyClicksOnPopupDuration =
    base::Milliseconds(500);

// By spec, dropdowns should always have a width which is a multiple of 12.
constexpr int kAutofillPopupWidthMultiple = 12;
constexpr int kAutofillPopupMinWidth = 0;
// TODO(crbug.com/831603): move handling the max width to the base class.
constexpr int kAutofillPopupMaxWidth = kAutofillPopupWidthMultiple * 38;

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

int GetContentsVerticalPadding() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTENT_LIST_VERTICAL_SINGLE);
}

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
  if (image_skia.isNull())
    return nullptr;
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
  if (icon_str.empty())
    return nullptr;

  // For http warning message, get icon images from VectorIcon, which is the
  // same as security indicator icons in location bar.
  if (icon_str == "httpWarning")
    return ImageViewFromVectorIcon(omnibox::kHttpIcon);

  if (icon_str == "httpsInvalid") {
    return std::make_unique<views::ImageView>(
        ui::ImageModel::FromVectorIcon(vector_icons::kNotSecureWarningIcon,
                                       ui::kColorAlertHighSeverity, kIconSize));
  }

  if (icon_str == "keyIcon")
    return ImageViewFromVectorIcon(kKeyIcon);

  if (icon_str == "clearIcon")
    return ImageViewFromVectorIcon(kBackspaceIcon);

  if (icon_str == "globeIcon")
    return ImageViewFromVectorIcon(kGlobeIcon);

  if (icon_str == "accountIcon")
    return ImageViewFromVectorIcon(kAccountCircleIcon);

  if (icon_str == "settingsIcon") {
    return ImageViewFromVectorIcon(
        base::FeatureList::IsEnabled(
            autofill::features::kAutofillUseConsistentPopupSettingsIcons)
            ? kMonoColorProductIcon
            : vector_icons::kSettingsIcon);
  }

  if (icon_str == "empty")
    return ImageViewFromVectorIcon(omnibox::kHttpIcon);

  if (icon_str == "device")
    return ImageViewFromVectorIcon(kLaptopAndSmartphoneIcon);

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
  if (icon_text == autofill::kAmericanExpressCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_AMEX);
  if (icon_text == autofill::kDinersCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_DINERS);
  if (icon_text == autofill::kDiscoverCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_DISCOVER);
  if (icon_text == autofill::kEloCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_ELO);
  if (icon_text == autofill::kJCBCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_JCB);
  if (icon_text == autofill::kMasterCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_MASTERCARD);
  if (icon_text == autofill::kMirCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_MIR);
  if (icon_text == autofill::kTroyCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_TROY);
  if (icon_text == autofill::kUnionPay)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_UNION_PAY);
  if (icon_text == autofill::kVisaCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_VISA);
  // Other networks.
  if (icon_text == autofill::kGenericCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_GENERIC);
  return std::u16string();
}

}  // namespace

namespace autofill {

namespace {

class PopupSeparator : public views::Separator {
 public:
  METADATA_HEADER(PopupSeparator);
  explicit PopupSeparator(AutofillPopupBaseView* popup);

 private:
  raw_ptr<AutofillPopupBaseView> popup_;
};

PopupSeparator::PopupSeparator(AutofillPopupBaseView* popup) : popup_(popup) {
  SetPreferredLength(views::MenuConfig::instance().separator_thickness);
  // Add some spacing between the previous item and the separator.
  // If the feature AutofillVisualImprovementsForSuggestionUi is enabled, also
  // add a padding after the separator.
  // TODO(crbug.com/1274134): Clean up once improvements are launched.
  SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      GetContentsVerticalPadding(), 0, GetContentsVerticalPadding(), 0)));
  SetColorId(popup_->GetSeparatorColorId());
}

BEGIN_METADATA(PopupSeparator, views::Separator)
END_METADATA

class SuggestionLabel : public views::Label {
 public:
  METADATA_HEADER(SuggestionLabel);
  SuggestionLabel(const std::u16string& text, AutofillPopupBaseView* popup);

  // views::Label:
  void OnThemeChanged() override;

 private:
  raw_ptr<AutofillPopupBaseView> popup_;
};

SuggestionLabel::SuggestionLabel(const std::u16string& text,
                                 AutofillPopupBaseView* popup)
    : Label(text,
            views::style::CONTEXT_DIALOG_BODY_TEXT,
            ChromeTextStyle::STYLE_RED),
      popup_(popup) {
  SetMultiLine(true);
  SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
}

void SuggestionLabel::OnThemeChanged() {
  views::Label::OnThemeChanged();
  SetEnabledColor(popup_->GetWarningColor());
}

BEGIN_METADATA(SuggestionLabel, views::Label)
END_METADATA

// This represents a single selectable item. Subclasses distinguish between
// footer and suggestion rows, which are structurally similar but have
// distinct styling.
class AutofillPopupItemView : public AutofillPopupRowView {
 public:
  METADATA_HEADER(AutofillPopupItemView);
  ~AutofillPopupItemView() override = default;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 protected:
  AutofillPopupItemView(AutofillPopupViewNativeViews* popup_view,
                        int line_number,
                        int frontend_id)
      : AutofillPopupRowView(popup_view, line_number),
        frontend_id_(frontend_id) {}

  // AutofillPopupRowView:
  void CreateContent() override;
  void RefreshStyle() override;
  std::unique_ptr<views::Background> CreateBackground() final;

  int GetFrontendId() const;

  virtual int GetPrimaryTextStyle() = 0;
  // Returns a main text label view. The label part is optional but allow caller
  // to keep track of all the labels for background color update.
  virtual std::unique_ptr<views::Label> CreateMainTextView();
  // Returns a minor text label view. The label is shown side by side with the
  // main text view, but in a secondary style. Can be nullptr.
  virtual std::unique_ptr<views::Label> CreateMinorTextView();
  // The description view can be nullptr.
  virtual std::unique_ptr<views::View> CreateDescriptionView();

  // Returns the font weight to be applied to primary info.
  virtual gfx::Font::Weight GetPrimaryTextWeight() const = 0;

  void AddSpacerWithSize(int spacer_width,
                         bool resize,
                         views::BoxLayout* layout);

  void KeepLabel(views::Label* label) {
    if (label)
      inner_labels_.push_back(label);
  }

  // Returns the string to be set as the name of the ui::AXNodeData.
  std::u16string GetVoiceOverString();

 private:
  // Returns a vector of optional labels to be displayed beneath value.
  virtual std::vector<std::unique_ptr<views::View>> CreateSubtextViews();

  // Returns the minimum cross axis size depending on the length of
  // GetSubtexts();
  void UpdateLayoutSize(views::BoxLayout* layout_manager, int64_t num_subtexts);

  // Returns true if the mouse is within the bounds of this item. This is not
  // affected by whether or not the item is overlaid by another popup.
  bool IsMouseInsideItemBounds() const { return IsMouseHovered(); }

  // We want a mouse click to accept a suggestion only if the user has made an
  // explicit choice. Therefore, we shall ignore mouse clicks unless the mouse
  // has been moved into the item's screen bounds. For example, if the item is
  // hovered by the mouse at the time it's first shown, we want to ignore clicks
  // until the mouse has left and re-entered the bounds of the item
  // (crbug.com/1240472, crbug.com/1241585, crbug.com/1287364).
  bool mouse_observed_outside_item_bounds_ = false;

  const int frontend_id_;

  // All the labels inside this view.
  std::vector<views::Label*> inner_labels_;
};

int AutofillPopupItemView::GetFrontendId() const {
  return frontend_id_;
}

BEGIN_METADATA(AutofillPopupItemView, AutofillPopupRowView)
ADD_READONLY_PROPERTY_METADATA(int, FrontendId)
END_METADATA

// This represents a suggestion, i.e., a row containing data that will be filled
// into the page if selected.
class AutofillPopupSuggestionView : public AutofillPopupItemView {
 public:
  METADATA_HEADER(AutofillPopupSuggestionView);
  AutofillPopupSuggestionView(const AutofillPopupSuggestionView&) = delete;
  AutofillPopupSuggestionView& operator=(const AutofillPopupSuggestionView&) =
      delete;
  ~AutofillPopupSuggestionView() override = default;

  static AutofillPopupSuggestionView* Create(
      AutofillPopupViewNativeViews* popup_view,
      int line_number,
      int frontend_id,
      PopupType popup_type);

 protected:
  // AutofillPopupItemView:
  int GetPrimaryTextStyle() override;
  gfx::Font::Weight GetPrimaryTextWeight() const override;
  std::unique_ptr<views::Label> CreateMainTextView() override;
  std::vector<std::unique_ptr<views::View>> CreateSubtextViews() override;
  AutofillPopupSuggestionView(AutofillPopupViewNativeViews* popup_view,
                              int line_number,
                              int frontend_id,
                              PopupType popup_type);

 private:
  // The popup type to which this suggestion belongs.
  PopupType popup_type_;
};

BEGIN_METADATA(AutofillPopupSuggestionView, AutofillPopupItemView)
END_METADATA

// This represents a password suggestion row, i.e., a username and password.
class PasswordPopupSuggestionView : public AutofillPopupSuggestionView {
 public:
  METADATA_HEADER(PasswordPopupSuggestionView);
  PasswordPopupSuggestionView(const PasswordPopupSuggestionView&) = delete;
  PasswordPopupSuggestionView& operator=(const PasswordPopupSuggestionView&) =
      delete;
  ~PasswordPopupSuggestionView() override = default;

  static PasswordPopupSuggestionView* Create(
      AutofillPopupViewNativeViews* popup_view,
      int line_number,
      int frontend_id);

 protected:
  // AutofillPopupItemView:
  std::unique_ptr<views::Label> CreateMainTextView() override;
  std::vector<std::unique_ptr<views::View>> CreateSubtextViews() override;
  std::unique_ptr<views::View> CreateDescriptionView() override;
  gfx::Font::Weight GetPrimaryTextWeight() const override;

 private:
  PasswordPopupSuggestionView(AutofillPopupViewNativeViews* popup_view,
                              int line_number,
                              int frontend_id);
  std::u16string origin_;
  std::u16string masked_password_;
};

BEGIN_METADATA(PasswordPopupSuggestionView, AutofillPopupSuggestionView)
END_METADATA

// This represents an option which appears in the footer of the dropdown, such
// as a row which will open the Autofill settings page when selected.
class AutofillPopupFooterView : public AutofillPopupItemView {
 public:
  METADATA_HEADER(AutofillPopupFooterView);
  ~AutofillPopupFooterView() override = default;

  static AutofillPopupFooterView* Create(
      AutofillPopupViewNativeViews* popup_view,
      int line_number,
      int frontend_id);

 protected:
  // AutofillPopupItemView:
  void CreateContent() override;
  void RefreshStyle() override;
  int GetPrimaryTextStyle() override;
  gfx::Font::Weight GetPrimaryTextWeight() const override;

 private:
  AutofillPopupFooterView(AutofillPopupViewNativeViews* popup_view,
                          int line_number,
                          int frontend_id);
};

BEGIN_METADATA(AutofillPopupFooterView, AutofillPopupItemView)
END_METADATA

// Draws a separator between sections of the dropdown, namely between datalist
// and Autofill suggestions. Note that this is NOT the same as the border on top
// of the footer section or the border between footer items.
class AutofillPopupSeparatorView : public AutofillPopupRowView {
 public:
  METADATA_HEADER(AutofillPopupSeparatorView);
  AutofillPopupSeparatorView(const AutofillPopupSeparatorView&) = delete;
  AutofillPopupSeparatorView& operator=(const AutofillPopupSeparatorView&) =
      delete;
  ~AutofillPopupSeparatorView() override = default;

  static AutofillPopupSeparatorView* Create(
      AutofillPopupViewNativeViews* popup_view,
      int line_number);

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override {}
  void OnMouseExited(const ui::MouseEvent& event) override {}
  void OnMouseReleased(const ui::MouseEvent& event) override {}

 protected:
  // AutofillPopupRowView:
  void CreateContent() override;
  void RefreshStyle() override;
  std::unique_ptr<views::Background> CreateBackground() override;

 private:
  AutofillPopupSeparatorView(AutofillPopupViewNativeViews* popup_view,
                             int line_number);
};

BEGIN_METADATA(AutofillPopupSeparatorView, AutofillPopupRowView)
END_METADATA

// Draws a row which contains a warning message.
class AutofillPopupWarningView : public AutofillPopupRowView {
 public:
  METADATA_HEADER(AutofillPopupWarningView);
  AutofillPopupWarningView(const AutofillPopupWarningView&) = delete;
  AutofillPopupWarningView& operator=(const AutofillPopupWarningView&) = delete;
  ~AutofillPopupWarningView() override = default;

  static AutofillPopupWarningView* Create(
      AutofillPopupViewNativeViews* popup_view,
      int line_number);

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override {}
  void OnMouseReleased(const ui::MouseEvent& event) override {}

 protected:
  // AutofillPopupRowView:
  void CreateContent() override;
  void RefreshStyle() override {}
  std::unique_ptr<views::Background> CreateBackground() override;

 private:
  AutofillPopupWarningView(AutofillPopupViewNativeViews* popup_view,
                           int line_number)
      : AutofillPopupRowView(popup_view, line_number) {}
};

BEGIN_METADATA(AutofillPopupWarningView, AutofillPopupRowView)
END_METADATA

/************** AutofillPopupItemView **************/

void AutofillPopupItemView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // Options are selectable.
  node_data->role = ax::mojom::Role::kListBoxOption;
  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                              GetSelected());

  // It is possible for the screen reader to request the a11y role of an
  // AutofillPopupItemView one final time when the Autofill popup gets hidden.
  // At this point the controller is already invalid. Therefore we skip all
  // steps that require a controller.
  base::WeakPtr<AutofillPopupController> controller =
      popup_view()->controller();
  if (!controller)
    return;

  node_data->SetNameChecked(GetVoiceOverString());

  // Compute set size and position in set, by checking the frontend_id of each
  // row, summing the number of interactive rows, and subtracting the number
  // of separators found before this row from its |pos_in_set|.
  int set_size = 0;
  int pos_in_set = GetLineNumber() + 1;
  for (int i = 0; i < controller->GetLineCount(); ++i) {
    if (controller->GetSuggestionAt(i).frontend_id ==
        autofill::POPUP_ITEM_ID_SEPARATOR) {
      if (i < GetLineNumber())
        --pos_in_set;
    } else {
      ++set_size;
    }
  }
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize, set_size);
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, pos_in_set);
}

void AutofillPopupItemView::OnPaint(gfx::Canvas* canvas) {
  AutofillPopupRowView::OnPaint(canvas);
  mouse_observed_outside_item_bounds_ |= !IsMouseInsideItemBounds();
}

void AutofillPopupItemView::OnMouseEntered(const ui::MouseEvent& event) {
  // OnMouseEntered() does not imply that the mouse had been outside of the
  // item's bounds before: OnMouseEntered() also fires if the mouse moves just a
  // little bit on the item. We don't want to show a preview in such a case.
  if (!mouse_observed_outside_item_bounds_)
    return;

  base::WeakPtr<AutofillPopupController> controller =
      popup_view()->controller();
  if (controller)
    controller->SetSelectedLine(GetLineNumber());
}

void AutofillPopupItemView::OnMouseExited(const ui::MouseEvent& event) {
  // OnMouseExited() does not imply that the mouse has left the item's screen
  // bounds: OnMouseExited() also fires (on Windows, at least) even when another
  // popup overlays this item (crbug.com/1287364).
  mouse_observed_outside_item_bounds_ |= !IsMouseInsideItemBounds();

  base::WeakPtr<AutofillPopupController> controller =
      popup_view()->controller();
  if (controller)
    controller->SelectionCleared();
}

void AutofillPopupItemView::OnMouseReleased(const ui::MouseEvent& event) {
  // Ignore mouse clicks unless the user made the explicit choice to selected
  // the current item.
  if (!mouse_observed_outside_item_bounds_)
    return;

  // Ignore clicks immediately after the popup was shown. This is to prevent
  // users accidentally accepting suggestions (crbug.com/1279268).
  if (popup_view()->time_delta_since_popup_shown() <=
      kIgnoreEarlyClicksOnPopupDuration) {
    return;
  }

  base::WeakPtr<AutofillPopupController> controller =
      popup_view()->controller();
  if (controller && event.IsOnlyLeftMouseButton() &&
      HitTestPoint(event.location())) {
    controller->AcceptSuggestion(GetLineNumber());
  }
}

void AutofillPopupItemView::OnGestureEvent(ui::GestureEvent* event) {
  base::WeakPtr<AutofillPopupController> controller =
      popup_view()->controller();
  if (!controller)
    return;
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

void AutofillPopupItemView::CreateContent() {
  base::WeakPtr<AutofillPopupController> controller =
      popup_view()->controller();

  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, AutofillPopupBaseView::GetHorizontalMargin())));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  std::vector<Suggestion> suggestions = controller->GetSuggestions();

  std::unique_ptr<views::ImageView> icon =
      GetIconImageView(suggestions[GetLineNumber()]);
  if (icon) {
    AddChildView(std::move(icon));
    AddSpacerWithSize(AutofillPopupBaseView::GetHorizontalPadding(),
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
    AddSpacerWithSize(AutofillPopupBaseView::GetHorizontalPadding(),
                      /*resize=*/true, layout_manager);
    AddChildView(std::move(trailing_icon));
  }
}

void AutofillPopupItemView::RefreshStyle() {
  SetBackground(CreateBackground());
  SkColor bk_color = GetSelected() ? popup_view()->GetSelectedBackgroundColor()
                                   : popup_view()->GetBackgroundColor();

  // Set style for each label in this view depending on current state since the
  // style isn't automatically adjusted after creation of the label.
  for (views::Label* label : inner_labels_) {
    label->SetAutoColorReadabilityEnabled(false);
    label->SetBackgroundColor(bk_color);

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

std::unique_ptr<views::Background> AutofillPopupItemView::CreateBackground() {
  return views::CreateSolidBackground(
      GetSelected() ? popup_view()->GetSelectedBackgroundColor()
                    : popup_view()->GetBackgroundColor());
}

std::unique_ptr<views::Label> AutofillPopupItemView::CreateMainTextView() {
  // TODO(crbug.com/831603): Remove elision responsibilities from controller.
  const Suggestion::Text& main_text =
      popup_view()->controller()->GetSuggestionAt(GetLineNumber()).main_text;
  if (!main_text.is_primary) {
    std::unique_ptr<views::Label> label = CreateLabelWithStyleAndContext(
        main_text.value, views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_PRIMARY);
    KeepLabel(label.get());
    return label;
  }

  std::unique_ptr<views::Label> label = CreateLabelWithStyleAndContext(
      popup_view()->controller()->GetSuggestionMainTextAt(GetLineNumber()),
      views::style::CONTEXT_DIALOG_BODY_TEXT, GetPrimaryTextStyle());

  const gfx::Font::Weight font_weight = GetPrimaryTextWeight();
  if (font_weight != label->font_list().GetFontWeight()) {
    label->SetFontList(label->font_list().DeriveWithWeight(font_weight));
  }

  KeepLabel(label.get());
  return label;
}

std::unique_ptr<views::Label> AutofillPopupItemView::CreateMinorTextView() {
  std::u16string text =
      popup_view()->controller()->GetSuggestionMinorTextAt(GetLineNumber());
  if (text.empty())
    return nullptr;

  std::unique_ptr<views::Label> label = CreateLabelWithStyleAndContext(
      text, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  KeepLabel(label.get());
  return label;
}

std::unique_ptr<views::View> AutofillPopupItemView::CreateDescriptionView() {
  return nullptr;
}

std::vector<std::unique_ptr<views::View>>
AutofillPopupItemView::CreateSubtextViews() {
  return {};
}

void AutofillPopupItemView::UpdateLayoutSize(views::BoxLayout* layout_manager,
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
    layout_manager->set_inside_border_insets(
        gfx::Insets::TLBR(kAutofillPopupAdditionalPadding,
                          AutofillPopupBaseView::GetHorizontalMargin(),
                          kAutofillPopupAdditionalPadding,
                          AutofillPopupBaseView::GetHorizontalMargin()));
  }
}

void AutofillPopupItemView::AddSpacerWithSize(int spacer_width,
                                              bool resize,
                                              views::BoxLayout* layout) {
  auto spacer = std::make_unique<views::View>();
  spacer->SetPreferredSize(gfx::Size(spacer_width, 1));
  layout->SetFlexForView(AddChildView(std::move(spacer)),
                         /*flex=*/resize ? 1 : 0,
                         /*use_min_size=*/true);
}

std::u16string AutofillPopupItemView::GetVoiceOverString() {
  base::WeakPtr<AutofillPopupController> controller =
      popup_view()->controller();

  auto suggestion = controller->GetSuggestionAt(GetLineNumber());

  if (suggestion.voice_over)
    return *suggestion.voice_over;

  std::vector<std::u16string> text;
  std::u16string icon_name = GetIconAccessibleName(suggestion.icon);
  if (!icon_name.empty())
    text.push_back(icon_name);

  auto main_text = controller->GetSuggestionMainTextAt(GetLineNumber());
  text.push_back(main_text);

  auto minor_text = controller->GetSuggestionMinorTextAt(GetLineNumber());
  if (!minor_text.empty())
    text.push_back(minor_text);

  std::vector<std::vector<Suggestion::Text>> labels =
      controller->GetSuggestionLabelsAt(GetLineNumber());
  for (std::vector<Suggestion::Text>& row : labels) {
    for (Suggestion::Text label : row) {
      // |label_text| is not populated for footers or autocomplete entries.
      if (!label.value.empty())
        text.push_back(std::move(label.value));
    }
  }

  if (!suggestion.additional_label.empty()) {
    // |additional_label| is only populated in a passwords context.
    text.push_back(suggestion.additional_label);
  }

  return base::JoinString(text, u" ");
}

/************** AutofillPopupSuggestionView **************/

// static
AutofillPopupSuggestionView* AutofillPopupSuggestionView::Create(
    AutofillPopupViewNativeViews* popup_view,
    int line_number,
    int frontend_id,
    PopupType popup_type) {
  AutofillPopupSuggestionView* result = new AutofillPopupSuggestionView(
      popup_view, line_number, frontend_id, popup_type);
  result->Init();
  return result;
}

int AutofillPopupSuggestionView::GetPrimaryTextStyle() {
  return views::style::TextStyle::STYLE_PRIMARY;
}

gfx::Font::Weight AutofillPopupSuggestionView::GetPrimaryTextWeight() const {
  return gfx::Font::Weight::NORMAL;
}

AutofillPopupSuggestionView::AutofillPopupSuggestionView(
    AutofillPopupViewNativeViews* popup_view,
    int line_number,
    int frontend_id,
    PopupType popup_type)
    : AutofillPopupItemView(popup_view, line_number, frontend_id),
      popup_type_(popup_type) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

std::unique_ptr<views::Label>
AutofillPopupSuggestionView::CreateMainTextView() {
  std::unique_ptr<views::Label> label =
      AutofillPopupItemView::CreateMainTextView();
  if (popup_type_ == PopupType::kAddresses) {
    label->SetMaximumWidthSingleLine(kAutofillPopupAddressProfileMaxWidth);
  } else if (popup_type_ == PopupType::kCreditCards &&
             popup_view()
                 ->controller()
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
AutofillPopupSuggestionView::CreateSubtextViews() {
  std::vector<std::unique_ptr<views::View>> subtext_view;
  for (const std::vector<Suggestion::Text>& label_row :
       popup_view()->controller()->GetSuggestionLabelsAt(GetLineNumber())) {
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
      if (label.value.empty())
        break;

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

/************** PasswordPopupSuggestionView **************/

PasswordPopupSuggestionView* PasswordPopupSuggestionView::Create(
    AutofillPopupViewNativeViews* popup_view,
    int line_number,
    int frontend_id) {
  PasswordPopupSuggestionView* result =
      new PasswordPopupSuggestionView(popup_view, line_number, frontend_id);
  result->Init();
  return result;
}

std::unique_ptr<views::Label>
PasswordPopupSuggestionView::CreateMainTextView() {
  std::unique_ptr<views::Label> label =
      AutofillPopupSuggestionView::CreateMainTextView();
  label->SetMaximumWidthSingleLine(kAutofillPopupUsernameMaxWidth);
  return label;
}

std::vector<std::unique_ptr<views::View>>
PasswordPopupSuggestionView::CreateSubtextViews() {
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
PasswordPopupSuggestionView::CreateDescriptionView() {
  if (origin_.empty())
    return nullptr;

  std::unique_ptr<views::Label> label = CreateLabelWithStyleAndContext(
      origin_, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  label->SetElideBehavior(gfx::ELIDE_HEAD);
  KeepLabel(label.get());
  label->SetMaximumWidthSingleLine(kAutofillPopupUsernameMaxWidth);
  return label;
}

gfx::Font::Weight PasswordPopupSuggestionView::GetPrimaryTextWeight() const {
  return gfx::Font::Weight::NORMAL;
}

PasswordPopupSuggestionView::PasswordPopupSuggestionView(
    AutofillPopupViewNativeViews* popup_view,
    int line_number,
    int frontend_id)
    : AutofillPopupSuggestionView(popup_view,
                                  line_number,
                                  frontend_id,
                                  PopupType::kPasswords) {
  std::vector<std::vector<Suggestion::Text>> labels =
      popup_view->controller()->GetSuggestionLabelsAt(line_number);
  if (!labels.empty()) {
    DCHECK_EQ(labels.size(), 1U);
    DCHECK_EQ(labels[0].size(), 1U);
    origin_ = std::move(labels[0][0].value);
  }

  masked_password_ =
      popup_view->controller()->GetSuggestionAt(line_number).additional_label;
}

/************** AutofillPopupFooterView **************/

// static
AutofillPopupFooterView* AutofillPopupFooterView::Create(
    AutofillPopupViewNativeViews* popup_view,
    int line_number,
    int frontend_id) {
  AutofillPopupFooterView* result =
      new AutofillPopupFooterView(popup_view, line_number, frontend_id);
  result->Init();
  return result;
}

void AutofillPopupFooterView::CreateContent() {
  base::WeakPtr<AutofillPopupController> controller =
      popup_view()->controller();

  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::VH(0, AutofillPopupBaseView::GetHorizontalMargin())));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  const Suggestion suggestion = controller->GetSuggestions()[GetLineNumber()];
  std::unique_ptr<views::ImageView> icon = GetIconImageView(suggestion);

  const bool use_leading_icon =
      base::Contains(kItemTypesUsingLeadingIcons, GetFrontendId());

  if (suggestion.is_loading) {
    SetEnabled(false);
    AddChildView(std::make_unique<views::Throbber>())->Start();
    AddSpacerWithSize(AutofillPopupBaseView::GetHorizontalPadding(),
                      /*resize=*/false, layout_manager);
  } else if (icon && use_leading_icon) {
    AddChildView(std::move(icon));
    AddSpacerWithSize(AutofillPopupBaseView::GetHorizontalPadding(),
                      /*resize=*/false, layout_manager);
  }

  layout_manager->set_minimum_cross_axis_size(
      views::MenuConfig::instance().touchable_menu_height);

  auto main_text_label = CreateMainTextView();
  main_text_label->SetEnabled(!suggestion.is_loading);
  AddChildView(std::move(main_text_label));

  AddSpacerWithSize(0, /*resize=*/true, layout_manager);

  if (icon && !use_leading_icon) {
    AddSpacerWithSize(AutofillPopupBaseView::GetHorizontalPadding(),
                      /*resize=*/false, layout_manager);
    AddChildView(std::move(icon));
  }

  std::unique_ptr<views::ImageView> trailing_icon =
      GetTrailingIconImageView(suggestion);
  if (trailing_icon) {
    AddSpacerWithSize(AutofillPopupBaseView::GetHorizontalPadding(),
                      /*resize=*/true, layout_manager);
    AddChildView(std::move(trailing_icon));
  }
}

void AutofillPopupFooterView::RefreshStyle() {
  AutofillPopupItemView::RefreshStyle();
  SetBorder(views::CreateSolidSidedBorder(
      gfx::Insets(),
      GetColorProvider()->GetColor(popup_view()->GetSeparatorColorId())));
}

int AutofillPopupFooterView::GetPrimaryTextStyle() {
  return views::style::STYLE_SECONDARY;
}

gfx::Font::Weight AutofillPopupFooterView::GetPrimaryTextWeight() const {
  return gfx::Font::Weight::NORMAL;
}

AutofillPopupFooterView::AutofillPopupFooterView(
    AutofillPopupViewNativeViews* popup_view,
    int line_number,
    int frontend_id)
    : AutofillPopupItemView(popup_view, line_number, frontend_id) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

/************** AutofillPopupSeparatorView **************/

// static
AutofillPopupSeparatorView* AutofillPopupSeparatorView::Create(
    AutofillPopupViewNativeViews* popup_view,
    int line_number) {
  AutofillPopupSeparatorView* result =
      new AutofillPopupSeparatorView(popup_view, line_number);
  result->Init();
  return result;
}

void AutofillPopupSeparatorView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  // Separators are not selectable.
  node_data->role = ax::mojom::Role::kSplitter;
}

void AutofillPopupSeparatorView::CreateContent() {
  SetUseDefaultFillLayout(true);
  AddChildView(std::make_unique<PopupSeparator>(popup_view()));
}

void AutofillPopupSeparatorView::RefreshStyle() {
  SetBackground(CreateBackground());
  SchedulePaint();
}

std::unique_ptr<views::Background>
AutofillPopupSeparatorView::CreateBackground() {
  return views::CreateSolidBackground(popup_view()->GetBackgroundColor());
}

AutofillPopupSeparatorView::AutofillPopupSeparatorView(
    AutofillPopupViewNativeViews* popup_view,
    int line_number)
    : AutofillPopupRowView(popup_view, line_number) {
  SetFocusBehavior(FocusBehavior::NEVER);
}

/************** AutofillPopupWarningView **************/

// static
AutofillPopupWarningView* AutofillPopupWarningView::Create(
    AutofillPopupViewNativeViews* popup_view,
    int line_number) {
  AutofillPopupWarningView* result =
      new AutofillPopupWarningView(popup_view, line_number);
  result->Init();
  return result;
}

void AutofillPopupWarningView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  base::WeakPtr<AutofillPopupController> controller =
      popup_view()->controller();
  if (!controller)
    return;

  node_data->role = ax::mojom::Role::kStaticText;
  node_data->SetNameChecked(
      controller->GetSuggestionAt(GetLineNumber()).main_text.value);
}

void AutofillPopupWarningView::CreateContent() {
  base::WeakPtr<AutofillPopupController> controller =
      popup_view()->controller();

  int horizontal_margin = AutofillPopupBaseView::GetHorizontalMargin();
  int vertical_margin = AutofillPopupBaseView::GetCornerRadius();

  SetUseDefaultFillLayout(true);
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(vertical_margin, horizontal_margin)));

  AddChildView(std::make_unique<SuggestionLabel>(
      controller->GetSuggestionMainTextAt(GetLineNumber()), popup_view()));
}

std::unique_ptr<views::Background>
AutofillPopupWarningView::CreateBackground() {
  return nullptr;
}

}  // namespace

/************** AutofillPopupRowView **************/
void AutofillPopupRowView::SetSelected(bool selected) {
  if (selected == selected_)
    return;

  selected_ = selected;
  if (selected)
    popup_view_->NotifyAXSelection(this);
  RefreshStyle();
  OnPropertyChanged(&selected_, views::kPropertyEffectsNone);
}

void AutofillPopupRowView::MaybeShowIphPromo() {
  std::string feature_name = popup_view()
                                 ->controller()
                                 ->GetSuggestionAt(GetLineNumber())
                                 .feature_for_iph;
  if (feature_name.empty())
    return;

  if (feature_name == "IPH_AutofillVirtualCardSuggestion") {
    SetProperty(views::kElementIdentifierKey,
                kAutofillCreditCardSuggestionEntryElementId);
    Browser* browser = popup_view()->browser();
    DCHECK(browser);
    browser->window()->MaybeShowFeaturePromo(
        feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);
  }
}

void AutofillPopupRowView::OnThemeChanged() {
  views::View::OnThemeChanged();
  RefreshStyle();
}

bool AutofillPopupRowView::OnMouseDragged(const ui::MouseEvent& event) {
  return true;
}

bool AutofillPopupRowView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

AutofillPopupRowView::AutofillPopupRowView(
    AutofillPopupViewNativeViews* popup_view,
    int line_number)
    : popup_view_(popup_view), line_number_(line_number) {
  SetNotifyEnterExitOnChild(true);
}

void AutofillPopupRowView::Init() {
  CreateContent();
}

int AutofillPopupRowView::GetLineNumber() const {
  return line_number_;
}

bool AutofillPopupRowView::GetSelected() const {
  return selected_;
}

bool AutofillPopupRowView::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  base::WeakPtr<AutofillPopupController> controller =
      popup_view()->controller();
  if (controller && action_data.action == ax::mojom::Action::kFocus)
    controller->SetSelectedLine(line_number_);
  return View::HandleAccessibleAction(action_data);
}

BEGIN_METADATA(AutofillPopupRowView, views::View)
ADD_PROPERTY_METADATA(bool, Selected)
ADD_READONLY_PROPERTY_METADATA(int, LineNumber)
END_METADATA

/************** AutofillPopupViewNativeViews **************/

AutofillPopupViewNativeViews::AutofillPopupViewNativeViews(
    base::WeakPtr<AutofillPopupController> controller,
    views::Widget* parent_widget)
    : AutofillPopupBaseView(controller, parent_widget),
      controller_(controller) {
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  layout_->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);

  CreateChildViews();
}

AutofillPopupViewNativeViews::~AutofillPopupViewNativeViews() = default;

void AutofillPopupViewNativeViews::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kListBox;
  // If controller_ is valid, then the view is expanded.
  if (controller_) {
    node_data->AddState(ax::mojom::State::kExpanded);
  } else {
    node_data->AddState(ax::mojom::State::kCollapsed);
    node_data->AddState(ax::mojom::State::kInvisible);
  }
  node_data->SetNameChecked(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_POPUP_ACCESSIBLE_NODE_DATA));
}

void AutofillPopupViewNativeViews::OnThemeChanged() {
  AutofillPopupBaseView::OnThemeChanged();
  SetBackground(views::CreateSolidBackground(GetBackgroundColor()));
  // |scroll_view_| and |footer_container_| will be null if there is no body
  // or footer content, respectively.
  if (scroll_view_) {
    scroll_view_->SetBackgroundColor(GetBackgroundColor());
  }
  if (footer_container_) {
    footer_container_->SetBackground(
        views::CreateSolidBackground(GetFooterBackgroundColor()));
  }
}

void AutofillPopupViewNativeViews::Show() {
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
  DoShow();
}

void AutofillPopupViewNativeViews::Hide() {
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
  // The controller is no longer valid after it hides us.
  controller_ = nullptr;
  DoHide();
}

void AutofillPopupViewNativeViews::OnSelectedRowChanged(
    absl::optional<int> previous_row_selection,
    absl::optional<int> current_row_selection) {
  if (previous_row_selection) {
    rows_[*previous_row_selection]->SetSelected(false);
  }

  if (current_row_selection) {
    AutofillPopupRowView* current_row = rows_[*current_row_selection];
    current_row->SetSelected(true);
    current_row->ScrollViewToVisible();
  }

  NotifyAccessibilityEvent(ax::mojom::Event::kSelectedChildrenChanged, true);
}

void AutofillPopupViewNativeViews::OnSuggestionsChanged() {
  CreateChildViews();
  DoUpdateBoundsAndRedrawPopup();
}

absl::optional<int32_t> AutofillPopupViewNativeViews::GetAxUniqueId() {
  return absl::optional<int32_t>(
      AutofillPopupBaseView::GetViewAccessibility().GetUniqueId());
}

void AutofillPopupViewNativeViews::AxAnnounce(const std::u16string& text) {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view)
    return;
  browser_view->GetViewAccessibility().AnnounceText(text);
}

void AutofillPopupViewNativeViews::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  if (visible) {
    for (auto* row_view : rows_)
      row_view->MaybeShowIphPromo();
  }
}

void AutofillPopupViewNativeViews::CreateChildViews() {
  RemoveAllChildViews();
  rows_.clear();
  scroll_view_ = nullptr;
  body_container_ = nullptr;
  footer_container_ = nullptr;

  int line_count = controller_->GetLineCount();

  std::vector<int> footer_item_line_numbers;
  footer_item_line_numbers.reserve(line_count);

  // Convert a line number to a front end id.
  auto line_number_to_frontend_id = [&](int line_number) {
    return controller_->GetSuggestionAt(line_number).frontend_id;
  };

  // Returns true if the item at |line_number| is a footer item.
  // Returns false if the |line_number| exceeds the line count.
  auto is_footer_item = [&](int line_number) {
    if (line_number >= line_count)
      return false;

    switch (line_number_to_frontend_id(line_number)) {
      case PopupItemId::POPUP_ITEM_ID_SCAN_CREDIT_CARD:
      case PopupItemId::POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO:
      case PopupItemId::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_EMPTY:
      case PopupItemId::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN:
      case PopupItemId::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_RE_SIGNIN:
      case PopupItemId::
          POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE:
      case PopupItemId::POPUP_ITEM_ID_SHOW_ACCOUNT_CARDS:
      case PopupItemId::POPUP_ITEM_ID_USE_VIRTUAL_CARD:
      case PopupItemId::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY:
      case PopupItemId::POPUP_ITEM_ID_CLEAR_FORM:
      case PopupItemId::POPUP_ITEM_ID_AUTOFILL_OPTIONS:
      case PopupItemId::POPUP_ITEM_ID_SEE_PROMO_CODE_DETAILS:
        return true;

      default:
        return false;
    }
  };

  // Process and add all the suggestions which are in the primary container.
  // Collect all footer line numbers such that those can be added to the footer
  // container below. DCHECK that all non-footer items are added before any
  // footer items.
  for (int current_line_number = 0; current_line_number < line_count;
       ++current_line_number) {
    // Collect all footer item numbers for subsequent processing.
    if (is_footer_item(current_line_number)) {
      footer_item_line_numbers.emplace_back(current_line_number);
      continue;
    }

    int frontend_id = line_number_to_frontend_id(current_line_number);
    bool is_separator = frontend_id == PopupItemId::POPUP_ITEM_ID_SEPARATOR;

    // Non-footer items must precede footer items.
    // The separator item can be part of both the footer or the scroll view.
    DCHECK(footer_item_line_numbers.empty() || is_separator);

    switch (frontend_id) {
      // The separator should be added to the scroll view only if the next item
      // is not a footer item and the footer section has not been started yet.
      case PopupItemId::POPUP_ITEM_ID_SEPARATOR:
        if (footer_item_line_numbers.empty() &&
            !is_footer_item(current_line_number + 1)) {
          rows_.push_back(
              AutofillPopupSeparatorView::Create(this, current_line_number));
        } else {
          footer_item_line_numbers.push_back(current_line_number);
        }
        break;

      case PopupItemId::POPUP_ITEM_ID_MIXED_FORM_MESSAGE:
      case PopupItemId::POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE:
        rows_.push_back(
            AutofillPopupWarningView::Create(this, current_line_number));
        break;

      case PopupItemId::POPUP_ITEM_ID_USERNAME_ENTRY:
      case PopupItemId::POPUP_ITEM_ID_PASSWORD_ENTRY:
      case PopupItemId::POPUP_ITEM_ID_ACCOUNT_STORAGE_USERNAME_ENTRY:
      case PopupItemId::POPUP_ITEM_ID_ACCOUNT_STORAGE_PASSWORD_ENTRY:
        rows_.push_back(PasswordPopupSuggestionView::Create(
            this, current_line_number, frontend_id));
        break;

      // The default section contains most of the suggestions including
      // addresses and credit cards.
      default:
        rows_.push_back(AutofillPopupSuggestionView::Create(
            this, current_line_number, frontend_id,
            controller_->GetPopupType()));
    }
  }

  // If kAutofillVisualImprovementsForSuggestionUi is enabled, introduce an
  // additional view with a vertical padding that wraps the full content of the
  // popup. This is similar to the padding_wrapper used in the scroll area, but
  // it allows to add a padding below the footer.
  // Create the view and set the convenience pointers defined above.
  std::unique_ptr<views::View> content_padding_wrapper =
      std::make_unique<views::View>();

  raw_ptr<views::BoxLayout> content_layout =
      content_padding_wrapper->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kVertical));

  // This adds a padding area on the top and the bottom of the popup content.
  content_padding_wrapper->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(GetContentsVerticalPadding(), 0)));

  raw_ptr<views::View> content_view =
      AddChildView(std::move(content_padding_wrapper));

  content_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  if (!rows_.empty()) {
    // Create a container to wrap the "regular" (non-footer) rows.
    std::unique_ptr<views::View> body_container =
        std::make_unique<views::View>();
    views::BoxLayout* body_layout =
        body_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    body_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);
    for (auto* row : rows_) {
      body_container->AddChildView(row);
    }

    scroll_view_ = new views::ScrollView();
    scroll_view_->SetHorizontalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
    body_container_ = scroll_view_->SetContents(std::move(body_container));
    scroll_view_->SetDrawOverflowIndicator(false);
    scroll_view_->ClipHeightTo(0, body_container_->GetPreferredSize().height());

    content_view->AddChildView(scroll_view_.get());
    content_layout->SetFlexForView(scroll_view_.get(), 1);
  }

  if (footer_item_line_numbers.empty()) {
    return;
  }

  // Footer items need to be in their own container because they should not be
  // affected by scrolling behavior (they are "sticky" at the bottom) and
  // because they have a special background color
  auto* footer_container = new views::View();

  views::BoxLayout* footer_layout =
      footer_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  footer_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  for (int line_number : footer_item_line_numbers) {
    // The footer can contain either footer views or separator lines.
    if (line_number_to_frontend_id(line_number) == POPUP_ITEM_ID_SEPARATOR) {
      rows_.push_back(AutofillPopupSeparatorView::Create(this, line_number));
    } else {
      rows_.push_back(AutofillPopupFooterView::Create(
          this, line_number, line_number_to_frontend_id(line_number)));
    }
    footer_container->AddChildView(rows_.back());
  }

  footer_container_ = content_view->AddChildView(footer_container);
  content_layout->SetFlexForView(footer_container_, 0);
}

int AutofillPopupViewNativeViews::AdjustWidth(int width) const {
  if (width >= kAutofillPopupMaxWidth)
    return kAutofillPopupMaxWidth;

  if (width <= kAutofillPopupMinWidth)
    return kAutofillPopupMinWidth;

  // The popup size is being determined by the contents, rather than the min/max
  // or the element bounds. Round up to a multiple of
  // |kAutofillPopupWidthMultiple|.
  if (width % kAutofillPopupWidthMultiple) {
    width +=
        (kAutofillPopupWidthMultiple - (width % kAutofillPopupWidthMultiple));
  }

  return width;
}

bool AutofillPopupViewNativeViews::DoUpdateBoundsAndRedrawPopup() {
  gfx::Size preferred_size = CalculatePreferredSize();
  gfx::Rect popup_bounds;

  const gfx::Rect content_area_bounds = GetContentAreaBounds();
  // TODO(crbug.com/1262371) Once popups can render outside the main window on
  // Linux, use the screen bounds.
  const gfx::Rect top_window_bounds = GetTopWindowBounds();
  const gfx::Rect& max_bounds_for_popup =
      PopupMayExceedContentAreaBounds(controller_->GetWebContents())
          ? top_window_bounds
          : content_area_bounds;

  gfx::Rect element_bounds =
      gfx::ToEnclosingRect(controller_->element_bounds());

  // If the element exceeds the content area, ensure that the popup is still
  // visually attached to the input element.
  element_bounds.Intersect(content_area_bounds);
  if (element_bounds.IsEmpty()) {
    controller_->Hide(PopupHidingReason::kElementOutsideOfContentArea);
    return false;
  }

  // Consider the element is |kElementBorderPadding| pixels larger at the top
  // and at the bottom in order to reposition the dropdown, so that it doesn't
  // look too close to the element.
  element_bounds.Inset(
      gfx::Insets::VH(/*vertical=*/-kElementBorderPadding, /*horizontal=*/0));

  // At least one row of the popup should be shown in the bounds of the content
  // area so that the user notices the presence of the popup.
  int item_height =
      body_container_ && body_container_->children().size() > 0
          ? body_container_->children()[0]->GetPreferredSize().height()
          : 0;

  if (!CanShowDropdownHere(item_height, max_bounds_for_popup, element_bounds)) {
    controller_->Hide(PopupHidingReason::kInsufficientSpace);
    return false;
  }

  CalculatePopupYAndHeight(preferred_size.height(), max_bounds_for_popup,
                           element_bounds, &popup_bounds);

  // Adjust the width to compensate for a scroll bar, if necessary, and for
  // other rules.
  int scroll_width = 0;
  if (scroll_view_ && preferred_size.height() > popup_bounds.height()) {
    preferred_size.set_height(popup_bounds.height());

    // Because the preferred size is greater than the bounds available, the
    // contents will have to scroll. The scroll bar will steal width from the
    // content and smoosh everything together. Instead, add to the width to
    // compensate.
    scroll_width = scroll_view_->GetScrollBarLayoutWidth();
  }
  preferred_size.set_width(AdjustWidth(preferred_size.width() + scroll_width));

  popup_bounds = GetOptionalPositionAndPlaceArrowOnPopup(
      element_bounds, content_area_bounds, preferred_size);

  if (BoundsOverlapWithAnyOpenPrompt(popup_bounds,
                                     controller_->GetWebContents())) {
    controller_->Hide(PopupHidingReason::kOverlappingWithAnotherPrompt);
    return false;
  }
  // On Windows, due to platform-specific implementation details, the previous
  // check isn't reliable, and fails to detect open prompts. Since the most
  // critical bubble is the permission bubble, we check for that specifically.
  if (BoundsOverlapWithOpenPermissionsPrompt(popup_bounds,
                                             controller_->GetWebContents())) {
    controller_->Hide(PopupHidingReason::kOverlappingWithAnotherPrompt);
    return false;
  }

  // The pip surface is given the most preference while rendering. So, the
  // autofill popup should not be shown when the picture in picture window
  // hides the autofill form behind it.
  // For more details on how this can happen, see crbug.com/1358647.
  if (BoundsOverlapWithPictureInPictureWindow(popup_bounds)) {
    controller_->Hide(
        PopupHidingReason::kOverlappingWithPictureInPictureWindow);
    return false;
  }

  SetSize(preferred_size);

  popup_bounds.Inset(-GetWidget()->GetRootView()->GetInsets());
  GetWidget()->SetBounds(popup_bounds);
  UpdateClipPath();

  SchedulePaint();
  return true;
}

BEGIN_METADATA(AutofillPopupViewNativeViews, AutofillPopupBaseView)
END_METADATA

// static
AutofillPopupView* AutofillPopupView::Create(
    base::WeakPtr<AutofillPopupController> controller) {
#if BUILDFLAG(IS_MAC)
  // It's possible for the container_view to not be in a window. In that case,
  // cancel the popup since we can't fully set it up.
  if (!platform_util::GetTopLevel(controller->container_view()))
    return nullptr;
#endif

  views::Widget* observing_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          controller->container_view());

#if !BUILDFLAG(IS_MAC)
  // If the top level widget can't be found, cancel the popup since we can't
  // fully set it up. On Mac Cocoa browser, |observing_widget| is null
  // because the parent is not a views::Widget.
  if (!observing_widget)
    return nullptr;
#endif

  return new AutofillPopupViewNativeViews(controller, observing_widget);
}

}  // namespace autofill
