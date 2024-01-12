// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/omnibox/browser/vector_icons.h"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "components/plus_addresses/resources/vector_icons.h"
#endif
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

// Metric to measure the duration of getting the image for the Autofill pop-up.
constexpr char kHistogramGetImageViewByName[] =
    "Autofill.PopupGetImageViewTime";

// Returns the name of the network for payment method icons, empty string
// otherwise.
std::u16string GetIconAccessibleName(Suggestion::Icon icon) {
  // Networks for which icons are currently shown.
  switch (icon) {
    case Suggestion::Icon::kCardAmericanExpress:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_AMEX);
    case Suggestion::Icon::kCardDiners:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_DINERS);
    case Suggestion::Icon::kCardDiscover:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_DISCOVER);
    case Suggestion::Icon::kCardElo:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_ELO);
    case Suggestion::Icon::kCardJCB:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_JCB);
    case Suggestion::Icon::kCardMasterCard:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_MASTERCARD);
    case Suggestion::Icon::kCardMir:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_MIR);
    case Suggestion::Icon::kCardTroy:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_TROY);
    case Suggestion::Icon::kCardUnionPay:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_UNION_PAY);
    case Suggestion::Icon::kCardVisa:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_VISA);
    // Other networks.
    case Suggestion::Icon::kCardGeneric:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_GENERIC);
    case Suggestion::Icon::kNoIcon:
    case Suggestion::Icon::kAccount:
    case Suggestion::Icon::kClear:
    case Suggestion::Icon::kCreate:
    case Suggestion::Icon::kCode:
    case Suggestion::Icon::kDelete:
    case Suggestion::Icon::kDevice:
    case Suggestion::Icon::kEdit:
    case Suggestion::Icon::kEmpty:
    case Suggestion::Icon::kGlobe:
    case Suggestion::Icon::kGoogle:
    case Suggestion::Icon::kGooglePasswordManager:
    case Suggestion::Icon::kGooglePay:
    case Suggestion::Icon::kGooglePayDark:
    case Suggestion::Icon::kHttpWarning:
    case Suggestion::Icon::kHttpsInvalid:
    case Suggestion::Icon::kKey:
    case Suggestion::Icon::kLocation:
    case Suggestion::Icon::kMagic:
    case Suggestion::Icon::kOfferTag:
    case Suggestion::Icon::kPenSpark:
    case Suggestion::Icon::kScanCreditCard:
    case Suggestion::Icon::kSettings:
    case Suggestion::Icon::kSettingsAndroid:
    case Suggestion::Icon::kUndo:
    case Suggestion::Icon::kPlusAddress:
      return std::u16string();
  }
  NOTREACHED_NORETURN();
}

std::unique_ptr<views::ImageView> ImageViewFromImageSkia(
    const gfx::ImageSkia& image_skia) {
  if (image_skia.isNull()) {
    return nullptr;
  }
  auto image_view = std::make_unique<views::ImageView>();
  image_view->SetImage(ui::ImageModel::FromImageSkia(image_skia));
  return image_view;
}

std::unique_ptr<views::ImageView> GetIconImageViewFromIcon(
    Suggestion::Icon icon) {
  switch (icon) {
    case Suggestion::Icon::kNoIcon:
      return nullptr;
    case Suggestion::Icon::kHttpWarning:
      // For the http warning message, get the icon images from VectorIcon,
      // which is the same as the security indicator icons in the location bar.
      return ImageViewFromVectorIcon(omnibox::kHttpIcon, kIconSize);
    case Suggestion::Icon::kHttpsInvalid:
      return std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kNotSecureWarningIcon, ui::kColorAlertHighSeverity,
          kIconSize));
    case Suggestion::Icon::kKey:
      return ImageViewFromVectorIcon(kKeyIcon, kIconSize);
    case Suggestion::Icon::kEdit:
      return ImageViewFromVectorIcon(vector_icons::kEditIcon, kIconSize);
    case Suggestion::Icon::kCode:
      return ImageViewFromVectorIcon(vector_icons::kCodeIcon, kIconSize);
    case Suggestion::Icon::kLocation:
      return ImageViewFromVectorIcon(vector_icons::kLocationOnIcon, kIconSize);
    case Suggestion::Icon::kDelete:
      return ImageViewFromVectorIcon(kTrashCanLightIcon, kIconSize);
    case Suggestion::Icon::kClear:
      return ImageViewFromVectorIcon(kBackspaceIcon, kIconSize);
    case Suggestion::Icon::kUndo:
      return ImageViewFromVectorIcon(vector_icons::kUndoIcon, kIconSize);
    case Suggestion::Icon::kGlobe:
      return ImageViewFromVectorIcon(kGlobeIcon, kIconSize);
    case Suggestion::Icon::kMagic:
      return ImageViewFromVectorIcon(vector_icons::kMagicButtonIcon, kIconSize);
    case Suggestion::Icon::kAccount:
      return ImageViewFromVectorIcon(kAccountCircleIcon, kIconSize);
    case Suggestion::Icon::kSettings:
      return ImageViewFromVectorIcon(omnibox::kProductIcon, kIconSize);
    case Suggestion::Icon::kEmpty:
      return ImageViewFromVectorIcon(omnibox::kHttpIcon, kIconSize);
    case Suggestion::Icon::kDevice:
      return ImageViewFromVectorIcon(kDevicesIcon, kIconSize);
    case Suggestion::Icon::kGoogle:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return ImageViewFromImageSkia(gfx::CreateVectorIcon(
          vector_icons::kGoogleGLogoIcon, kIconSize, gfx::kPlaceholderColor));
#else
      return nullptr;
#endif
    case Suggestion::Icon::kPenSpark:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return ImageViewFromVectorIcon(vector_icons::kPenSparkIcon, kIconSize);
#else
      return nullptr;
#endif
    case Suggestion::Icon::kGooglePasswordManager:
      return ImageViewFromVectorIcon(GooglePasswordManagerVectorIcon(),
                                     kGooglePasswordManagerIconSize);
    case Suggestion::Icon::kPlusAddress:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return ImageViewFromVectorIcon(plus_addresses::kPlusAddressesLogoIcon,
                                     kIconSize);
#else
      return ImageViewFromVectorIcon(vector_icons::kEmailIcon, kIconSize);
#endif
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case Suggestion::Icon::kGooglePay:
    case Suggestion::Icon::kGooglePayDark:
      return nullptr;
#else
    case Suggestion::Icon::kGooglePay:
    case Suggestion::Icon::kGooglePayDark:
#endif
    case Suggestion::Icon::kCreate:
    case Suggestion::Icon::kOfferTag:
    case Suggestion::Icon::kScanCreditCard:
    case Suggestion::Icon::kSettingsAndroid:
    case Suggestion::Icon::kCardGeneric:
    case Suggestion::Icon::kCardAmericanExpress:
    case Suggestion::Icon::kCardDiners:
    case Suggestion::Icon::kCardDiscover:
    case Suggestion::Icon::kCardElo:
    case Suggestion::Icon::kCardJCB:
    case Suggestion::Icon::kCardMasterCard:
    case Suggestion::Icon::kCardMir:
    case Suggestion::Icon::kCardTroy:
    case Suggestion::Icon::kCardUnionPay:
    case Suggestion::Icon::kCardVisa:
      // For other suggestion entries, get the icon from PNG files.
      int icon_id = GetIconResourceID(icon);
      DCHECK_NE(icon_id, 0);
      return ImageViewFromImageSkia(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(icon_id));
  }
  NOTREACHED_NORETURN();
}

std::u16string GetSuggestionA11yCoreMessage(const Suggestion& suggestion) {
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

}  // namespace

std::u16string GetVoiceOverStringFromSuggestion(const Suggestion& suggestion) {
  std::vector<std::u16string> text({GetSuggestionA11yCoreMessage(suggestion)});

  if (!suggestion.children.empty()) {
    CHECK(IsExpandablePopupItemId(suggestion.popup_item_id));

    if (suggestion.popup_item_id == PopupItemId::kAddressEntry) {
      text.push_back(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_EXPANDABLE_SUGGESTION_FULL_ADDRESS_A11Y_ADDON));
    }

    std::u16string shortcut = l10n_util::GetStringUTF16(
        base::i18n::IsRTL()
            ? IDS_AUTOFILL_EXPANDABLE_SUGGESTION_EXPAND_SHORTCUT_RTL
            : IDS_AUTOFILL_EXPANDABLE_SUGGESTION_EXPAND_SHORTCUT);

    text.push_back(l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_EXPANDABLE_SUGGESTION_SUBMENU_HINT, shortcut));
  }

  return base::JoinString(text, u". ");
}

gfx::Insets GetMarginsForContentCell(bool has_control_element) {
  int left_margin = PopupBaseView::GetHorizontalMargin();
  int right_margin = left_margin;
  if (ShouldApplyNewAutofillPopupStyle()) {
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
  base::TimeTicks start_time = base::TimeTicks::Now();

  if (!suggestion.custom_icon.IsEmpty()) {
    return ImageViewFromImageSkia(suggestion.custom_icon.AsImageSkia());
  }
  std::unique_ptr<views::ImageView> icon_image_view =
      GetIconImageViewFromIcon(suggestion.icon);
  base::UmaHistogramTimes(kHistogramGetImageViewByName,
                          base::TimeTicks::Now() - start_time);

  return icon_image_view;
}

std::unique_ptr<views::ImageView> GetTrailingIconImageView(
    const Suggestion& suggestion) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  std::unique_ptr<views::ImageView> icon_image_view =
      GetIconImageViewFromIcon(suggestion.trailing_icon);
  base::UmaHistogramTimes(kHistogramGetImageViewByName,
                          base::TimeTicks::Now() - start_time);

  return icon_image_view;
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
    PopupRowContentView& content_view) {
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
    PopupRowContentView& content_view) {
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
  content_view.UpdateStyle(/*selected=*/false);
}

void FormatLabel(views::Label& label,
                 const Suggestion::Text& text,
                 PopupType popup_type) {
  if (popup_type == PopupType::kAddresses) {
    label.SetMaximumWidthSingleLine(kAutofillPopupAddressProfileMaxWidth);
  } else if (popup_type == PopupType::kCreditCards &&
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

std::vector<std::unique_ptr<views::View>> CreateAndTrackSubtextViews(
    PopupRowContentView& content_view,
    const Suggestion& suggestion,
    PopupType popup_type,
    int text_style) {
  std::vector<std::unique_ptr<views::View>> result;
  const int kHorizontalSpacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);

  for (const std::vector<Suggestion::Text>& label_row : suggestion.labels) {
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
              ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL, text_style));
      content_view.TrackLabel(label);
      FormatLabel(*label, label_text, popup_type);
    }
    result.push_back(std::move(label_row_container_view));
  }

  return result;
}

void AddSuggestionStrategyContentCellChildren(PopupRowContentView* view,
                                              const Suggestion& suggestion,
                                              PopupType popup_type) {
  // Add the label views.
  std::unique_ptr<views::Label> main_text_label = CreateMainTextLabel(
      suggestion.main_text, views::style::TextStyle::STYLE_PRIMARY);
  FormatLabel(*main_text_label, suggestion.main_text, popup_type);
  AddSuggestionContentToView(
      suggestion, std::move(main_text_label),
      CreateMinorTextLabel(suggestion.minor_text),
      /*description_label=*/nullptr,
      CreateAndTrackSubtextViews(*view, suggestion, popup_type), *view);
}

std::unique_ptr<views::ImageView> ImageViewFromVectorIcon(
    const gfx::VectorIcon& vector_icon,
    int icon_size = kIconSize) {
  return std::make_unique<views::ImageView>(
      ui::ImageModel::FromVectorIcon(vector_icon, ui::kColorIcon, icon_size));
}

}  // namespace autofill::popup_cell_utils
