// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/autofill/autofill_popup_controller_utils.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/view.h"

namespace autofill::popup_cell_utils {

namespace {

// The default icon size used in the suggestion drop down.
constexpr int kIconSize = 16;

// Max width for the username and masked password.
constexpr int kAutofillPopupUsernameMaxWidth = 272;

// Max width for address profile suggestion text.
constexpr int kAutofillPopupAddressProfileMaxWidth = 192;
// Max width for address credit card suggestion text.
constexpr int kAutofillPopupCreditCardMaxWidth = 192;

// The additional height of the row in case it has two lines of text.
constexpr int kAutofillPopupAdditionalDoubleRowHeight = 22;

// The additional padding of the row in case it has three lines of text.
constexpr int kAutofillPopupAdditionalPadding = 16;

// Vertical spacing between labels in one row.
constexpr int kAdjacentLabelsVerticalSpacing = 2;

// The icon size used in the suggestion dropdown for displaying the Google
// Password Manager icon in the Manager Passwords entry.
constexpr int kGooglePasswordManagerIconSize = 20;

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

std::unique_ptr<views::ImageView> ImageViewFromVectorIcon(
    const gfx::VectorIcon& vector_icon,
    int icon_size = kIconSize) {
  return std::make_unique<views::ImageView>(
      ui::ImageModel::FromVectorIcon(vector_icon, ui::kColorIcon, icon_size));
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

std::unique_ptr<views::ImageView> GetIconImageViewByName(
    const std::string& icon_str) {
  if (icon_str.empty()) {
    return nullptr;
  }

  // For http warning message, get icon images from VectorIcon, which is the
  // same as security indicator icons in location bar.
  if (icon_str == "httpWarning") {
    return ImageViewFromVectorIcon(omnibox::kHttpIcon, kIconSize);
  }

  if (icon_str == "httpsInvalid") {
    return std::make_unique<views::ImageView>(
        ui::ImageModel::FromVectorIcon(vector_icons::kNotSecureWarningIcon,
                                       ui::kColorAlertHighSeverity, kIconSize));
  }

  if (icon_str == "keyIcon") {
    return ImageViewFromVectorIcon(kKeyIcon, kIconSize);
  }

  if (icon_str == "deleteIcon") {
    return ImageViewFromVectorIcon(kTrashCanLightIcon, kIconSize);
  }

  if (icon_str == "clearIcon") {
    return ImageViewFromVectorIcon(kBackspaceIcon, kIconSize);
  }

  if (icon_str == "undoIcon") {
    return ImageViewFromVectorIcon(vector_icons::kUndoIcon, kIconSize);
  }

  if (icon_str == "globeIcon") {
    return ImageViewFromVectorIcon(kGlobeIcon, kIconSize);
  }

  // TODO(crbug.com/1459990): Use proper icon. The magic_button icon does not
  // exist yet, I will introduce it in a follow up cl.
  if (icon_str == "magicIcon") {
    return ImageViewFromVectorIcon(kGlobeIcon, kIconSize);
  }

  if (icon_str == "accountIcon") {
    return ImageViewFromVectorIcon(kAccountCircleIcon, kIconSize);
  }

  if (icon_str == "settingsIcon") {
    return ImageViewFromVectorIcon(omnibox::kProductIcon, kIconSize);
  }

  if (icon_str == "empty") {
    return ImageViewFromVectorIcon(omnibox::kHttpIcon, kIconSize);
  }

  if (icon_str == "device") {
    return ImageViewFromVectorIcon(kDevicesIcon, kIconSize);
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
  int icon_id = GetIconResourceID(icon_str);
  DCHECK_NE(icon_id, 0);
  return ImageViewFromImageSkia(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(icon_id));
}

}  // namespace

std::u16string GetVoiceOverStringFromSuggestion(const Suggestion& suggestion) {
  if (suggestion.voice_over) {
    return *suggestion.voice_over;
  }

  std::vector<std::u16string> text;
  auto add_if_not_empty = [&text](std::u16string value) {
    if (!value.empty()) {
      text.push_back(std::move(value));
    }
  };

  add_if_not_empty(GetIconAccessibleName(suggestion.icon));
  text.push_back(suggestion.main_text.value);
  add_if_not_empty(suggestion.minor_text.value);

  for (const std::vector<Suggestion::Text>& row : suggestion.labels) {
    for (const Suggestion::Text& label : row) {
      // `label_text` is not populated for footers or autocomplete entries.
      add_if_not_empty(label.value);
    }
  }

  // `additional_label` is only populated in a passwords context.
  add_if_not_empty(suggestion.additional_label);

  return base::JoinString(text, u" ");
}

gfx::Insets GetMarginsForContentCell(bool has_control_element) {
  int left_margin = PopupBaseView::GetHorizontalMargin();
  int right_margin = left_margin;

  if (base::FeatureList::IsEnabled(
          features::kAutofillShowAutocompleteDeleteButton)) {
    // If the feature is enabled, then the row already adds some extra
    // horizontal margin on the left - deduct that.
    left_margin = std::max(
        0, left_margin - ChromeLayoutProvider::Get()->GetDistanceMetric(
                             DISTANCE_CONTENT_LIST_VERTICAL_SINGLE));

    // If there is no control element, then this is the only cell and the same
    // correction needs to be made on the right side, too.
    if (!has_control_element) {
      right_margin = left_margin;
    }
  }
  return gfx::Insets::TLBR(0, left_margin, 0, right_margin);
}

std::unique_ptr<views::ImageView> GetIconImageView(
    const Suggestion& suggestion) {
  if (!suggestion.custom_icon.IsEmpty()) {
    return ImageViewFromImageSkia(suggestion.custom_icon.AsImageSkia());
  }

  return GetIconImageViewByName(suggestion.icon);
}

std::unique_ptr<views::ImageView> GetTrailingIconImageView(
    const Suggestion& suggestion) {
  return GetIconImageViewByName(suggestion.trailing_icon);
}

// Adds a spacer with `spacer_width` to `view`. `layout` must be the
// LayoutManager of `view`.
void AddSpacerWithSize(views::View& view,
                       views::BoxLayout& layout,
                       int spacer_width,
                       bool resize) {
  auto spacer = views::Builder<views::View>()
                    .SetPreferredSize(gfx::Size(spacer_width, 1))
                    .Build();
  layout.SetFlexForView(view.AddChildView(std::move(spacer)),
                        /*flex=*/resize ? 1 : 0,
                        /*use_min_size=*/true);
}

// Creates the table in which all  the Autofill suggestion content apart from
// leading and trailing icons is contained and adds it to `content_view`.
// It registers `main_text_label`, `minor_text_label`, and `description_label`
// with `content_view` for tracking, but assumes that the labels inside of of
// `subtext_views` have already been registered for tracking with
// `content_view`.
void AddSuggestionContentTableToView(
    std::unique_ptr<views::Label> main_text_label,
    std::unique_ptr<views::Label> minor_text_label,
    std::unique_ptr<views::Label> description_label,
    std::vector<std::unique_ptr<views::View>> subtext_views,
    PopupCellView& content_view) {
  const int kDividerSpacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);
  auto content_table =
      views::Builder<views::TableLayoutView>()
          .AddColumn(views::LayoutAlignment::kStart,
                     views::LayoutAlignment::kStretch,
                     views::TableLayout::kFixedSize,
                     views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
          .AddPaddingColumn(views::TableLayout::kFixedSize, kDividerSpacing)
          .AddColumn(views::LayoutAlignment::kStart,
                     views::LayoutAlignment::kStretch,
                     views::TableLayout::kFixedSize,
                     views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
          .Build();

  // Major and minor text go into the first row, first column.
  content_table->AddRows(1, 0);
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

    content_view.TrackLabel(
        first_line_container->AddChildView(std::move(main_text_label)));
    content_view.TrackLabel(
        first_line_container->AddChildView(std::move(minor_text_label)));
    content_table->AddChildView(std::move(first_line_container));
  } else {
    content_view.TrackLabel(
        content_table->AddChildView(std::move(main_text_label)));
  }

  // The description goes into the first row, second column.
  if (description_label) {
    content_view.TrackLabel(
        content_table->AddChildView(std::move(description_label)));
  } else {
    content_table->AddChildView(std::make_unique<views::View>());
  }

  // Every subtext label goes into an additional row.
  for (std::unique_ptr<views::View>& subtext_view : subtext_views) {
    content_table->AddPaddingRow(0, kAdjacentLabelsVerticalSpacing)
        .AddRows(1, 0);
    content_table->AddChildView(std::move(subtext_view));
    content_table->AddChildView(std::make_unique<views::View>());
  }
  content_view.AddChildView(std::move(content_table));
}

// Creates the content structure shared by autocomplete, address, credit card,
// and password suggestions.
// - `minor_text_label`, `description_label`, and `subtext_labels` may all be
// null or empty.
// - `content_view` is the (assumed to be empty) view to which the content
// structure for the `suggestion` is added.
void AddSuggestionContentToView(
    const Suggestion& suggestion,
    std::unique_ptr<views::Label> main_text_label,
    std::unique_ptr<views::Label> minor_text_label,
    std::unique_ptr<views::Label> description_label,
    std::vector<std::unique_ptr<views::View>> subtext_views,
    PopupCellView& content_view) {
  views::BoxLayout& layout =
      *content_view.SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          GetMarginsForContentCell(/*has_control_element=*/false)));

  layout.set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Adjust the cell height based on the number of subtexts.
  const int kStandardRowHeight =
      views::MenuConfig::instance().touchable_menu_height;
  const int kActualHeight =
      kStandardRowHeight +
      (subtext_views.empty() ? 0 : kAutofillPopupAdditionalDoubleRowHeight);
  layout.set_minimum_cross_axis_size(kActualHeight);

  // If there are three rows in total, add extra padding to avoid cramming.
  DCHECK_LE(subtext_views.size(), 2u);
  if (subtext_views.size() == 2u) {
    layout.set_inside_border_insets(gfx::Insets::VH(
        kAutofillPopupAdditionalPadding, PopupBaseView::GetHorizontalMargin()));
  }

  // The leading icon.
  if (std::unique_ptr<views::ImageView> icon = GetIconImageView(suggestion)) {
    content_view.AddChildView(std::move(icon));
    AddSpacerWithSize(content_view, layout,
                      PopupBaseView::GetHorizontalPadding(),
                      /*resize=*/false);
  }

  // The actual content table.
  AddSuggestionContentTableToView(
      std::move(main_text_label), std::move(minor_text_label),
      std::move(description_label), std::move(subtext_views), content_view);

  // The trailing icon.
  if (std::unique_ptr<views::ImageView> trailing_icon =
          GetTrailingIconImageView(suggestion)) {
    AddSpacerWithSize(content_view, layout,
                      PopupBaseView::GetHorizontalPadding(),
                      /*resize=*/true);
    content_view.AddChildView(std::move(trailing_icon));
  }

  // Force a refresh to ensure all the labels'styles are correct.
  content_view.RefreshStyle();
}

void FormatLabel(views::Label& label,
                 const Suggestion::Text& text,
                 base::WeakPtr<const AutofillPopupController> controller) {
  DCHECK(controller);
  if (controller->GetPopupType() == PopupType::kAddresses) {
    label.SetMaximumWidthSingleLine(kAutofillPopupAddressProfileMaxWidth);
  } else if (controller->GetPopupType() == PopupType::kCreditCards &&
             text.should_truncate.value()) {
    // should_truncate should only be set to true iff the experiments are
    // enabled.
    DCHECK(base::FeatureList::IsEnabled(
        autofill::features::kAutofillEnableVirtualCardMetadata));
    DCHECK(base::FeatureList::IsEnabled(
        autofill::features::kAutofillEnableCardProductName));
    label.SetMaximumWidthSingleLine(kAutofillPopupCreditCardMaxWidth);
  }
}

// Creates a label for the suggestion's main text.
std::unique_ptr<views::Label> CreateMainTextLabel(
    const Suggestion::Text& main_text,
    int text_style) {
  return std::make_unique<views::Label>(
      main_text.value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      !main_text.is_primary ? views::style::STYLE_PRIMARY : text_style);
}

// Creates a label for the suggestion's minor text.
std::unique_ptr<views::Label> CreateMinorTextLabel(
    const Suggestion::Text& minor_text) {
  return minor_text.value.empty()
             ? nullptr
             : std::make_unique<views::Label>(
                   minor_text.value, views::style::CONTEXT_DIALOG_BODY_TEXT,
                   views::style::STYLE_SECONDARY);
}

std::unique_ptr<views::Label> CreateDescriptionLabel(
    PopupCellView& content_view,
    base::WeakPtr<AutofillPopupController> controller,
    int line_number) {
  const Suggestion& kSuggestion = controller->GetSuggestionAt(line_number);
  if (kSuggestion.labels.empty()) {
    return nullptr;
  }

  DCHECK_EQ(kSuggestion.labels.size(), 1u);
  DCHECK_EQ(kSuggestion.labels[0].size(), 1u);

  auto label = std::make_unique<views::Label>(
      kSuggestion.labels[0][0].value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  label->SetElideBehavior(gfx::ELIDE_HEAD);
  label->SetMaximumWidthSingleLine(kAutofillPopupUsernameMaxWidth);
  return label;
}

std::vector<std::unique_ptr<views::View>> CreateAndTrackSubtextViews(
    PopupCellView& content_view,
    base::WeakPtr<AutofillPopupController> controller,
    int line_number) {
  std::vector<std::unique_ptr<views::View>> result;
  const int kHorizontalSpacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);

  for (const std::vector<Suggestion::Text>& label_row :
       controller->GetSuggestionAt(line_number).labels) {
    DCHECK_LE(label_row.size(), 2u);
    DCHECK(!label_row.empty());
    if (base::ranges::all_of(label_row, &std::u16string::empty,
                             &Suggestion::Text::value)) {
      // If a row is empty, do not include any further rows.
      return result;
    }

    auto label_row_container_view =
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
            .SetBetweenChildSpacing(kHorizontalSpacing)
            .Build();
    for (const Suggestion::Text& label_text : label_row) {
      // If a column is empty, do not include any further columns.
      if (label_text.value.empty()) {
        break;
      }
      auto* label =
          label_row_container_view->AddChildView(std::make_unique<views::Label>(
              label_text.value,
              ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
              views::style::STYLE_SECONDARY));
      content_view.TrackLabel(label);
      FormatLabel(*label, label_text, controller);
    }
    result.push_back(std::move(label_row_container_view));
  }

  return result;
}

// Adds the callbacks for the content area to `content_view`.
void AddCallbacksToContentView(
    base::WeakPtr<AutofillPopupController> controller,
    int line_number,
    PopupCellView& content_view) {
  content_view.SetOnSelectedCallback(base::BindRepeating(
      &AutofillPopupController::SelectSuggestion, controller, line_number));
  content_view.SetOnUnselectedCallback(base::BindRepeating(
      &AutofillPopupController::SelectSuggestion, controller, absl::nullopt));
  content_view.SetOnAcceptedCallback(base::BindRepeating(
      &AutofillPopupController::AcceptSuggestion, controller, line_number));
}

void AddSuggestionStrategyContentCellChildren(
    PopupCellView* view,
    base::WeakPtr<AutofillPopupController> controller,
    int line_number) {
  DCHECK(controller);
  const Suggestion& kSuggestion = controller->GetSuggestionAt(line_number);
  // Add the label views.
  std::unique_ptr<views::Label> main_text_label = CreateMainTextLabel(
      kSuggestion.main_text, views::style::TextStyle::STYLE_PRIMARY);
  FormatLabel(*main_text_label, kSuggestion.main_text, controller);
  AddSuggestionContentToView(
      kSuggestion, std::move(main_text_label),
      CreateMinorTextLabel(kSuggestion.minor_text),
      /*description_label=*/nullptr,
      CreateAndTrackSubtextViews(*view, controller, line_number), *view);

  // Prepare the callbacks to the controller.
  AddCallbacksToContentView(controller, line_number, *view);
}

}  // namespace autofill::popup_cell_utils
