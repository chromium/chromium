// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/image_model_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "components/plus_addresses/resources/vector_icons.h"
#endif

namespace autofill::popup_cell_utils {

namespace {

// The default icon size used in the suggestion drop down.
constexpr int kIconSize = 16;
constexpr int kChromeRefreshIconSize = 20;

// The additional height of the row in case it has two lines of text.
constexpr int kAutofillPopupAdditionalDoubleRowHeight = 16;

// The additional padding of the row in case it has three lines of text.
constexpr int kAutofillPopupAdditionalVerticalPadding = 16;

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
    case Suggestion::Icon::kCardVerve:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_VERVE);
    case Suggestion::Icon::kCardVisa:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_VISA);
    // Other networks.
    case Suggestion::Icon::kCardGeneric:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_GENERIC);

    case Suggestion::Icon::kAutofillPredictionImprovements:
    case Suggestion::Icon::kAutofillPredictionImprovementsDark:

    case Suggestion::Icon::kAccount:
    case Suggestion::Icon::kClear:
    case Suggestion::Icon::kCode:
    case Suggestion::Icon::kCreate:
    case Suggestion::Icon::kDelete:
    case Suggestion::Icon::kDevice:
    case Suggestion::Icon::kEdit:
    case Suggestion::Icon::kEmail:
    case Suggestion::Icon::kEmpty:
    case Suggestion::Icon::kError:
    case Suggestion::Icon::kGlobe:
    case Suggestion::Icon::kGoogle:
    case Suggestion::Icon::kGoogleMonochrome:
    case Suggestion::Icon::kGooglePasswordManager:
    case Suggestion::Icon::kGooglePay:
    case Suggestion::Icon::kGooglePayDark:
    case Suggestion::Icon::kHttpsInvalid:
    case Suggestion::Icon::kHttpWarning:
    case Suggestion::Icon::kIban:
    case Suggestion::Icon::kKey:
    case Suggestion::Icon::kLocation:
    case Suggestion::Icon::kMagic:
    case Suggestion::Icon::kNoIcon:
    case Suggestion::Icon::kOfferTag:
    case Suggestion::Icon::kPenSpark:
    case Suggestion::Icon::kPlusAddress:
    case Suggestion::Icon::kScanCreditCard:
    case Suggestion::Icon::kSettings:
    case Suggestion::Icon::kSettingsAndroid:
    case Suggestion::Icon::kUndo:
      return std::u16string();
  }
  NOTREACHED();
}

std::optional<ui::ImageModel> ImageModelFromImageSkia(
    const gfx::ImageSkia& image_skia) {
  if (image_skia.isNull()) {
    return std::nullopt;
  }
  auto image_model = ui::ImageModel::FromImageSkia(image_skia);
  return image_model;
}

// Converts the `image_model` to an `ImageView`. If `apply_deactivated_style` is
// true, the image will be converted to a disabled image.
std::unique_ptr<views::ImageView> ConvertModelToImageView(
    std::optional<ui::ImageModel> image_model,
    bool apply_deactivated_style) {
  if (!image_model) {
    return nullptr;
  }
  if (apply_deactivated_style) {
    image_model = ui::GetDefaultDisabledIconFromImageModel(
        image_model.value(),
        ui::ColorProviderManager::Get().GetColorProviderFor(
            ui::NativeTheme::GetInstanceForNativeUi()->GetColorProviderKey(
                nullptr)));
  }
  return std::make_unique<views::ImageView>(image_model.value());
}

// Creates the table in which all the Autofill suggestion content apart from
// leading and trailing icons is contained.
std::unique_ptr<views::TableLayoutView> CreateSuggestionContentTable(
    std::unique_ptr<views::Label> main_text_label,
    std::unique_ptr<views::Label> minor_text_label,
    std::unique_ptr<views::Label> description_label,
    std::vector<std::unique_ptr<views::View>> subtext_views) {
  const bool kHasTwoColumns = !!description_label;
  auto table =
      views::Builder<views::TableLayoutView>()
          .AddColumn(views::LayoutAlignment::kStart,
                     views::LayoutAlignment::kStretch,
                     views::TableLayout::kFixedSize,
                     views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
          .Build();
  if (kHasTwoColumns) {
    const int kDividerSpacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);
    table->AddPaddingColumn(views::TableLayout::kFixedSize, kDividerSpacing);
    table->AddColumn(views::LayoutAlignment::kStart,
                     views::LayoutAlignment::kStretch,
                     views::TableLayout::kFixedSize,
                     views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
  }

  // Major and minor text go into the first row, first column.
  table->AddRows(1, 0);
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
    table->AddChildView(std::move(first_line_container));
  } else {
    table->AddChildView(std::move(main_text_label));
  }

  // The description goes into the first row, second column.
  if (kHasTwoColumns) {
    table->AddChildView(description_label ? std::move(description_label)
                                          : std::make_unique<views::View>());
  }

  // Every subtext label goes into an additional row.
  for (std::unique_ptr<views::View>& subtext_view : subtext_views) {
    table->AddPaddingRow(0, kAdjacentLabelsVerticalSpacing).AddRows(1, 0);
    table->AddChildView(std::move(subtext_view));
    if (kHasTwoColumns) {
      table->AddChildView(std::make_unique<views::View>());
    }
  }
  return table;
}

}  // namespace

std::optional<ui::ImageModel> GetIconImageModelFromIcon(Suggestion::Icon icon) {
  switch (icon) {
    case Suggestion::Icon::kNoIcon:
      return std::nullopt;
    case Suggestion::Icon::kAccount:
      return ImageModelFromVectorIcon(kAccountCircleIcon, kIconSize);
    case Suggestion::Icon::kClear:
      return ImageModelFromVectorIcon(kBackspaceIcon, kIconSize);
    case Suggestion::Icon::kCode:
      return ImageModelFromVectorIcon(vector_icons::kCodeIcon, kIconSize);
    case Suggestion::Icon::kDelete:
      return ImageModelFromVectorIcon(kTrashCanRefreshIcon,
                                      kChromeRefreshIconSize);
    case Suggestion::Icon::kDevice:
      return ImageModelFromVectorIcon(kDevicesIcon, kIconSize);
    case Suggestion::Icon::kEdit:
      return ImageModelFromVectorIcon(vector_icons::kEditChromeRefreshIcon,
                                      kChromeRefreshIconSize);
    case Suggestion::Icon::kEmail:
      return ImageModelFromVectorIcon(vector_icons::kEmailOutlineIcon,
                                      kIconSize);
    case Suggestion::Icon::kEmpty:
      return ImageModelFromVectorIcon(omnibox::kHttpIcon, kIconSize);
    case Suggestion::Icon::kError:
      return ui::ImageModel::FromVectorIcon(vector_icons::kErrorIcon,
                                            ui::kColorSysError, kIconSize);
    case Suggestion::Icon::kGlobe:
      return ImageModelFromVectorIcon(kGlobeIcon, kIconSize);
    case Suggestion::Icon::kGoogle:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return ImageModelFromImageSkia(gfx::CreateVectorIcon(
          vector_icons::kGoogleGLogoIcon, kIconSize, gfx::kPlaceholderColor));
#else
      return std::nullopt;
#endif
    case Suggestion::Icon::kGoogleMonochrome:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return ImageModelFromVectorIcon(vector_icons::kGoogleGLogoMonochromeIcon,
                                      kIconSize);
#else
      return ImageModelFromVectorIcon(vector_icons::kEmailIcon, kIconSize);
#endif
    case Suggestion::Icon::kHttpWarning:
      // For the http warning message, get the icon images from VectorIcon,
      // which is the same as the security indicator icons in the location bar.
      return ImageModelFromVectorIcon(omnibox::kHttpIcon, kIconSize);
    case Suggestion::Icon::kHttpsInvalid:
      return ui::ImageModel::FromVectorIcon(vector_icons::kNotSecureWarningIcon,
                                            ui::kColorAlertHighSeverity,
                                            kIconSize);
    case Suggestion::Icon::kKey:
      return ImageModelFromVectorIcon(kKeyIcon, kIconSize);
    case Suggestion::Icon::kLocation:
      return ImageModelFromVectorIcon(
          vector_icons::kLocationOnChromeRefreshIcon, kChromeRefreshIconSize);
    case Suggestion::Icon::kMagic:
      return ImageModelFromVectorIcon(vector_icons::kMagicButtonIcon,
                                      kIconSize);
    case Suggestion::Icon::kPenSpark:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return ImageModelFromVectorIcon(vector_icons::kPenSparkIcon, kIconSize);
#else
      return ImageModelFromVectorIcon(vector_icons::kEditIcon, kIconSize);
#endif
    case Suggestion::Icon::kPlusAddress:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return ImageModelFromVectorIcon(plus_addresses::kPlusAddressLogoSmallIcon,
                                      kIconSize);
#else
      return ImageModelFromVectorIcon(vector_icons::kEmailIcon, kIconSize);
#endif
    case Suggestion::Icon::kSettings:
      return ImageModelFromVectorIcon(omnibox::kProductIcon, kIconSize);
    case Suggestion::Icon::kUndo:
      return ImageModelFromVectorIcon(vector_icons::kUndoIcon, kIconSize);
    case Suggestion::Icon::kGooglePasswordManager:
      return ImageModelFromVectorIcon(GooglePasswordManagerVectorIcon(),
                                      kGooglePasswordManagerIconSize);
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case Suggestion::Icon::kGooglePay:
    case Suggestion::Icon::kGooglePayDark:
      return std::nullopt;
#else
    case Suggestion::Icon::kGooglePay:
    case Suggestion::Icon::kGooglePayDark:
#endif
    case Suggestion::Icon::kIban:
    case Suggestion::Icon::kAutofillPredictionImprovements:
    case Suggestion::Icon::kAutofillPredictionImprovementsDark:
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
    case Suggestion::Icon::kCardVerve:
    case Suggestion::Icon::kCardVisa:
      // For other suggestion entries, get the icon from PNG files.
      int icon_id = GetIconResourceID(icon);
      DCHECK_NE(icon_id, 0);
      return ImageModelFromImageSkia(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(icon_id));
  }
  NOTREACHED();
}

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

std::unique_ptr<views::ImageView> GetIconImageView(
    const Suggestion& suggestion) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  if (auto* icon = absl::get_if<gfx::Image>(&suggestion.custom_icon);
      icon && !icon->IsEmpty()) {
    std::optional<ui::ImageModel> image_model =
        ImageModelFromImageSkia(icon->AsImageSkia());
    return ConvertModelToImageView(image_model,
                                   suggestion.apply_deactivated_style);
  }
  std::unique_ptr<views::ImageView> icon_image_view =
      ConvertModelToImageView(GetIconImageModelFromIcon(suggestion.icon),
                              suggestion.apply_deactivated_style);
  base::UmaHistogramTimes(kHistogramGetImageViewByName,
                          base::TimeTicks::Now() - start_time);

  if (icon_image_view) {
    // It is possible to have icons of different sizes (kChromeRefreshIconSize
    // and kIconSize) on the same popup. Setting the icon view width to
    // the largest value ensures that the icon occupies consistent horizontal
    // space and makes icons (and the text after them) aligned. It expands
    // the area of kIconSize icons only and doesn't change those that are bigger
    // by design (e.g. payment card icons) and have no alignment issues.
    gfx::Size size = icon_image_view->GetPreferredSize();
    size.set_width(std::max(kChromeRefreshIconSize, size.width()));
    icon_image_view->SetPreferredSize(size);
  }

  return icon_image_view;
}

std::unique_ptr<views::ImageView> GetTrailingIconImageView(
    const Suggestion& suggestion) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  std::optional<ui::ImageModel> image_model =
      GetIconImageModelFromIcon(suggestion.trailing_icon);
  std::unique_ptr<views::ImageView> icon_image_view =
      ConvertModelToImageView(image_model, suggestion.apply_deactivated_style);
  base::UmaHistogramTimes(kHistogramGetImageViewByName,
                          base::TimeTicks::Now() - start_time);

  return icon_image_view;
}

void AddSpacerWithSize(views::BoxLayoutView& view,
                       int spacer_width,
                       bool resize) {
  auto spacer = views::Builder<views::View>()
                    .SetPreferredSize(gfx::Size(spacer_width, 1))
                    .Build();
  view.SetFlexForView(view.AddChildView(std::move(spacer)),
                      /*flex=*/resize ? 1 : 0,
                      /*use_min_size=*/true);
}

void AddSuggestionContentToView(
    const Suggestion& suggestion,
    std::unique_ptr<views::Label> main_text_label,
    std::unique_ptr<views::Label> minor_text_label,
    std::unique_ptr<views::Label> description_label,
    std::vector<std::unique_ptr<views::View>> subtext_views,
    std::unique_ptr<views::View> icon,
    PopupRowContentView& content_view) {
  // Adjust the row height based on the number of subtexts (lines of text).
  int row_height = views::MenuConfig::instance().touchable_menu_height;
  if (!subtext_views.empty()) {
    row_height += kAutofillPopupAdditionalDoubleRowHeight;
  }
  content_view.SetMinimumCrossAxisSize(row_height);

  // If there are three rows in total, add extra padding to avoid cramming.
  DCHECK_LE(subtext_views.size(), 2u);
  if (subtext_views.size() == 2u) {
    content_view.SetInsideBorderInsets(
        gfx::Insets(content_view.GetInsideBorderInsets())
            .set_top_bottom(kAutofillPopupAdditionalVerticalPadding,
                            kAutofillPopupAdditionalVerticalPadding));
  }

  // The leading icon.
  if (suggestion.is_loading) {
    views::Throbber* throbber =
        content_view.AddChildView(std::make_unique<views::Throbber>());
    if (icon) {
      // Prevent that the layout is shifted when transitioning from throbber to
      // icon and vice versa when there is a width difference.
      const int size_delta =
          icon->GetMinimumSize().width() - throbber->GetMinimumSize().width();
      throbber->SetProperty(views::kMarginsKey,
                            gfx::Insets::VH(0, std::max(size_delta, 0) / 2));
    }
    throbber->Start();
    AddSpacerWithSize(content_view, PopupBaseView::ArrowHorizontalMargin(),
                      /*resize=*/false);
    content_view.SetEnabled(false);
  } else if (icon) {
    content_view.AddChildView(std::move(icon));
    AddSpacerWithSize(content_view, PopupBaseView::ArrowHorizontalMargin(),
                      /*resize=*/false);
  }

  // The actual content table.
  content_view.SetFlexForView(
      content_view.AddChildView(CreateSuggestionContentTable(
          std::move(main_text_label), std::move(minor_text_label),
          std::move(description_label), std::move(subtext_views))),
      1);

  // The trailing icon.
  if (std::unique_ptr<views::ImageView> trailing_icon =
          GetTrailingIconImageView(suggestion)) {
    AddSpacerWithSize(content_view, PopupBaseView::ArrowHorizontalMargin(),
                      /*resize=*/true);
    content_view.AddChildView(std::move(trailing_icon));
  }

  // Force a refresh to ensure all the labels'styles are correct.
  content_view.UpdateStyle(/*selected=*/false);
}

ui::ImageModel ImageModelFromVectorIcon(const gfx::VectorIcon& vector_icon,
                                        int icon_size = kIconSize) {
  return ui::ImageModel::FromVectorIcon(vector_icon, ui::kColorIcon, icon_size);
}

const gfx::VectorIcon& GetExpandableMenuIcon(SuggestionType type) {
  CHECK(IsExpandableSuggestionType(type));
  // Only compose suggestions have a different expandable icon.
  return GetFillingProductFromSuggestionType(type) == FillingProduct::kCompose
             ? kBrowserToolsChromeRefreshIcon
             : vector_icons::kSubmenuArrowChromeRefreshIcon;
}

}  // namespace autofill::popup_cell_utils
