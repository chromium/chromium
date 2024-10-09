// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_utils.h"
#include "chrome/browser/ui/views/autofill/popup/lazy_loading_image_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_prediction_improvements_feedback_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_with_button_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/favicon_base/favicon_types.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/new_badge_controller.h"
#include "components/user_education/views/new_badge_label.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace autofill {

namespace {

constexpr int kCustomIconSize = 16;

// The size of a close or delete icon.
constexpr int kCloseIconSize = 16;

// The size of a refresh icon.
constexpr int kRefreshIconSize = 16;
constexpr int kRefreshInkDropRadius = 12;

// Popup items that use a leading icon instead of a trailing one.
constexpr auto kPopupItemTypesUsingLeadingIcons =
    base::MakeFixedFlatSet<SuggestionType>(
        {SuggestionType::kAllSavedPasswordsEntry,
         SuggestionType::kDeleteAddressProfile,
         SuggestionType::kEditAddressProfile,
         SuggestionType::kFillEverythingFromAddressProfile,
         SuggestionType::kManageAddress, SuggestionType::kManageCreditCard,
         SuggestionType::kManageIban, SuggestionType::kManagePlusAddress,
         SuggestionType::kPasswordAccountStorageEmpty,
         SuggestionType::kPasswordAccountStorageOptIn,
         SuggestionType::kPasswordAccountStorageOptInAndGenerate,
         SuggestionType::kPasswordAccountStorageReSignin,
         SuggestionType::kShowAccountCards, SuggestionType::kUndoOrClear,
         SuggestionType::kViewPasswordDetails,
         SuggestionType::kRetrievePredictionImprovements});

// Max width for the username and masked password.
constexpr int kAutofillPopupUsernameMaxWidth = 272;
constexpr int kAutofillPopupPasswordMaxWidth = 108;

// Max width for the Autofill suggestion text.
constexpr int kAutofillSuggestionMaxWidth = 192;

// Max width for address profile suggestion text when granular filling is
// enabled.
constexpr int kAutofillPopupAddressProfileGranularFillingEnabledMaxWidth = 320;

constexpr auto kMainTextStyle = views::style::TextStyle::STYLE_BODY_3_MEDIUM;
constexpr auto kMainTextStyleLight = views::style::TextStyle::STYLE_BODY_3;
constexpr auto kMainTextStyleHighlighted =
    views::style::TextStyle::STYLE_BODY_3_BOLD;
constexpr auto kMinorTextStyle = views::style::TextStyle::STYLE_BODY_4;
constexpr auto kDisabledTextStyle = views::style::TextStyle::STYLE_DISABLED;

// Returns a wrapper around `closure` that posts it to the default message
// queue instead of executing it directly. This is to avoid that the callback's
// caller can suicide by (unwittingly) deleting itself or its parent.
base::RepeatingClosure CreateExecuteSoonWrapper(base::RepeatingClosure task) {
  return base::BindRepeating(
      [](base::RepeatingClosure delayed_task) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(delayed_task));
      },
      std::move(task));
}

bool IsDeactivatedPasswordOrPasskey(const Suggestion& suggestion) {
  return suggestion.apply_deactivated_style &&
         GetFillingProductFromSuggestionType(suggestion.type) ==
             FillingProduct::kPassword;
}

void FormatLabel(views::Label& label,
                 const Suggestion::Text& text,
                 FillingProduct main_filling_product,
                 int maximum_width_single_line) {
  switch (main_filling_product) {
    case FillingProduct::kAddress:
    case FillingProduct::kAutocomplete:
    case FillingProduct::kPlusAddresses:
      label.SetMaximumWidthSingleLine(maximum_width_single_line);
      break;
    case FillingProduct::kCreditCard:
    case FillingProduct::kStandaloneCvc:
      if (text.should_truncate.value()) {
        // should_truncate should only be set to true iff the experiments are
        // enabled.
        DCHECK(base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableVirtualCardMetadata));
        DCHECK(base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableCardProductName));
        label.SetMaximumWidthSingleLine(maximum_width_single_line);
      }
      break;
    case FillingProduct::kCompose:
    case FillingProduct::kIban:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kPassword:
    case FillingProduct::kPredictionImprovements:
    case FillingProduct::kNone:
      break;
  }
}

int GetMaxPopupAddressProfileWidth(bool should_use_new_popup_max_size) {
  return should_use_new_popup_max_size
             ? kAutofillPopupAddressProfileGranularFillingEnabledMaxWidth
             : kAutofillSuggestionMaxWidth;
}

// Returns true when `features::kAutofillGranularFillingAvailable` is true or
// the user is using manual fallbacks for unclassified address or payments
// fields. Note that `is_suggestion_acceptable` is not enough because
// suggestions like `SuggestionType::kDevtoolsTestAddresses` are also not
// acceptable. This method will always return the same values for all
// suggestions in a popup because:
// 1. If the user triggers address manual fallback on an unclassified field, all
// available suggestions will be of such type (address filling product, not
// acceptable and not a `SuggestionType::kDevtoolsTestAddresses` suggestion).
// 2. Same goes for credit card manual fallback suggestions. They will not be
// mixed with other ones.
// 3. If `features::kAutofillGranularFillingAvailable` is true, then this method
// also returns true regardless of the `FillingProduct` and whether a suggestion
// is acceptable.
//
// Note `SuggestionType::kDevtoolsTestAddresses` is not
// acceptable, and is the parent of `SuggestionType::kDevtoolsTestAddressEntry`
// (which can be accepted). We specifically remove it because its filling
// product is also `FillingProduct::kAddress`.
// TODO(crbug.com/40274514): Remove once clean up happens.
bool ShouldApplyNewPopupMaxWidth(SuggestionType suggestion_type,
                                 bool is_suggestion_acceptable) {
  FillingProduct filling_product =
      GetFillingProductFromSuggestionType(suggestion_type);
  const bool is_address_unclassified_field_manual_fallback =
      !is_suggestion_acceptable &&
      filling_product == FillingProduct::kAddress &&
      suggestion_type != SuggestionType::kDevtoolsTestAddresses;
  const bool is_credit_card_unclassified_field_manual_fallback =
      !is_suggestion_acceptable &&
      filling_product == FillingProduct::kCreditCard;
  return filling_product == FillingProduct::kPlusAddresses ||
         is_address_unclassified_field_manual_fallback ||
         is_credit_card_unclassified_field_manual_fallback ||
         base::FeatureList::IsEnabled(
             features::kAutofillGranularFillingAvailable);
}

// Creates a label for the suggestion's main text.
std::unique_ptr<views::Label> CreateMainTextLabel(
    const Suggestion& suggestion,
    std::optional<user_education::DisplayNewBadge> show_new_badge,
    views::style::TextStyle primary_text_style = kMainTextStyle) {
  views::style::TextStyle main_text_label_style;
  if (suggestion.apply_deactivated_style) {
    main_text_label_style = kDisabledTextStyle;
  } else {
    main_text_label_style = suggestion.main_text.is_primary
                                ? primary_text_style
                                : kMainTextStyleLight;
  }

  auto label = std::make_unique<user_education::NewBadgeLabel>(
      suggestion.main_text.value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      main_text_label_style);
  if (show_new_badge.has_value()) {
    label->SetDisplayNewBadge(show_new_badge.value());
  }

  if (!suggestion.main_text.is_primary) {
    label->SetEnabledColorId(ui::kColorLabelForegroundSecondary);
  }
  return label;
}

// Creates a label for the suggestion's minor text.
std::unique_ptr<views::Label> CreateMinorTextLabel(
    const Suggestion& suggestion) {
  if (suggestion.minor_text.value.empty()) {
    return nullptr;
  }
  auto label = std::make_unique<views::Label>(
      suggestion.minor_text.value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      suggestion.apply_deactivated_style ? kDisabledTextStyle
                                         : kMinorTextStyle);
  label->SetEnabledColorId(ui::kColorLabelForegroundSecondary);
  return label;
}

// Creates sub-text views and passes their references to `PopupRowContentView`
// for centralized style management.
std::vector<std::unique_ptr<views::View>> CreateSubtextViews(
    PopupRowContentView& content_view,
    const Suggestion& suggestion,
    FillingProduct main_filling_product) {
  std::vector<std::unique_ptr<views::View>> result;
  const int kHorizontalSpacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);

  for (const std::vector<Suggestion::Text>& label_row : suggestion.labels) {
    if (std::ranges::all_of(label_row, &std::u16string::empty,
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
              IsDeactivatedPasswordOrPasskey(suggestion) ? kDisabledTextStyle
                                                         : kMinorTextStyle));
      if (suggestion.type == SuggestionType::kPlusAddressError) {
        label->SetEnabledColorId(ui::kColorSysError);
      } else if (!IsDeactivatedPasswordOrPasskey(suggestion)) {
        label->SetEnabledColorId(ui::kColorLabelForegroundSecondary);
      }
      // To make sure the popup width will not exceed its maximum value,
      // divide the maximum label width by the number of labels.
      // TODO(crbug.com/40274514): Keep new behaviour where the max
      // popup size cannot be exceeded once clean up happens.
      const bool should_apply_new_popup_max_width = ShouldApplyNewPopupMaxWidth(
          suggestion.type, suggestion.is_acceptable);
      int shrink_formatted_label_by =
          should_apply_new_popup_max_width ? label_row.size() : 1;
      FormatLabel(
          *label, label_text, main_filling_product,
          GetMaxPopupAddressProfileWidth(should_apply_new_popup_max_width) /
              shrink_formatted_label_by);
    }
    result.push_back(std::move(label_row_container_view));
  }

  return result;
}

std::unique_ptr<PopupRowContentView> CreateFooterPopupRowContentView(
    const Suggestion& suggestion) {
  auto view = std::make_unique<PopupRowContentView>();

  std::unique_ptr<views::ImageView> icon =
      popup_cell_utils::GetIconImageView(suggestion);

  const bool kUseLeadingIcon =
      kPopupItemTypesUsingLeadingIcons.contains(suggestion.type);

  if (suggestion.is_loading) {
    view->AddChildView(std::make_unique<views::Throbber>())->Start();
    popup_cell_utils::AddSpacerWithSize(*view,
                                        PopupBaseView::ArrowHorizontalMargin(),
                                        /*resize=*/false);
  } else if (icon && kUseLeadingIcon) {
    view->AddChildView(std::move(icon));
    popup_cell_utils::AddSpacerWithSize(*view,
                                        PopupBaseView::ArrowHorizontalMargin(),
                                        /*resize=*/false);
  }

  view->SetMinimumCrossAxisSize(
      views::MenuConfig::instance().touchable_menu_height);

  std::unique_ptr<views::Label> main_text_label = CreateMainTextLabel(
      suggestion, /*show_new_badge=*/std::nullopt, kMainTextStyleLight);
  // TODO(crbug.com/345709988): Move this to CreateMainTextLabel. See
  // https://crrev.com/c/5605735/comment/970405c2_cbb55e85
  if (!suggestion.apply_deactivated_style) {
    main_text_label->SetEnabledColorId(ui::kColorLabelForegroundSecondary);
  }
  main_text_label->SetEnabled(!suggestion.is_loading);
  view->AddChildView(std::move(main_text_label));

  popup_cell_utils::AddSpacerWithSize(*view,
                                      /*spacer_width=*/0,
                                      /*resize=*/true);

  if (icon && !kUseLeadingIcon) {
    popup_cell_utils::AddSpacerWithSize(*view,
                                        PopupBaseView::ArrowHorizontalMargin(),
                                        /*resize=*/false);
    view->AddChildView(std::move(icon));
  }

  std::unique_ptr<views::ImageView> trailing_icon =
      popup_cell_utils::GetTrailingIconImageView(suggestion);
  if (trailing_icon) {
    popup_cell_utils::AddSpacerWithSize(*view,
                                        PopupBaseView::ArrowHorizontalMargin(),
                                        /*resize=*/true);
    view->AddChildView(std::move(trailing_icon));
  }

  // Force a refresh to ensure all the labels' styles are correct.
  view->UpdateStyle(/*selected=*/false);

  return view;
}

std::unique_ptr<views::Label> CreatePasswordDescriptionLabel(
    const Suggestion& suggestion) {
  if (suggestion.additional_label.empty()) {
    return nullptr;
  }
  auto label = std::make_unique<views::Label>(
      suggestion.additional_label, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  label->SetElideBehavior(gfx::ELIDE_HEAD);
  label->SetMaximumWidthSingleLine(kAutofillPopupUsernameMaxWidth);
  return label;
}

std::unique_ptr<views::View> CreatePasswordSubtextView(
    const Suggestion& suggestion) {
  CHECK_EQ(suggestion.labels.size(), 1u);
  CHECK_EQ(suggestion.labels[0].size(), 1u);

  const auto& label = suggestion.labels[0][0].value;
  auto label_view = std::make_unique<views::Label>(
      label, views::style::CONTEXT_DIALOG_BODY_TEXT,
      IsDeactivatedPasswordOrPasskey(suggestion)
          ? kDisabledTextStyle
          : views::style::STYLE_SECONDARY);
  // Password labels are obfuscated using password replacement character. Manual
  // fallback suggestions display credential username, which is not obfuscated
  // and should not be truncated.
  const bool is_password_label =
      label.find_first_not_of(
          password_manager::constants::kPasswordReplacementChar) ==
      std::u16string::npos;
  if (is_password_label) {
    label_view->SetElideBehavior(gfx::TRUNCATE);
    label_view->SetMaximumWidthSingleLine(kAutofillPopupPasswordMaxWidth);
  } else {
    label_view->SetElideBehavior(gfx::ELIDE_HEAD);
    label_view->SetMaximumWidthSingleLine(kAutofillPopupUsernameMaxWidth);
  }
  return label_view;
}

// If the `Suggestion::custom_icon` holds the `FaviconDetails` alternative,
// the icon should be loaded lazily. For this case, this method creates a
// `LazyLoadingImageView` to be passed to `CreatePasswordPopupRowContentView()`
// as the `icon`. Otherwise, it returns an icon created by `GetIconImageView()`.
std::unique_ptr<views::View> GetPasswordIconView(
    const Suggestion& suggestion,
    PasswordFaviconLoader* favicon_loader) {
  if (!absl::holds_alternative<Suggestion::FaviconDetails>(
          suggestion.custom_icon)) {
    return popup_cell_utils::GetIconImageView(suggestion);
  }

  CHECK(favicon_loader);
  std::optional<ui::ImageModel> suggestion_icon_model =
      popup_cell_utils::GetIconImageModelFromIcon(suggestion.icon);
  ui::ImageModel placeholder_icon =
      suggestion_icon_model ? std::move(*suggestion_icon_model)
                            : popup_cell_utils::ImageModelFromVectorIcon(
                                  kGlobeIcon, kCustomIconSize);

  return std::make_unique<LazyLoadingImageView>(
      gfx::Size(kCustomIconSize, kCustomIconSize), std::move(placeholder_icon),
      base::BindOnce(
          &PasswordFaviconLoader::Load, base::Unretained(favicon_loader),
          absl::get<Suggestion::FaviconDetails>(suggestion.custom_icon)));
}

std::unique_ptr<PopupRowContentView> CreatePasswordPopupRowContentView(
    const Suggestion& suggestion,
    std::optional<user_education::DisplayNewBadge> show_new_badge,
    std::optional<AutofillPopupController::SuggestionFilterMatch> filter_match,
    PasswordFaviconLoader* favicon_loader) {
  auto view = std::make_unique<PopupRowContentView>();

  // Add the actual views.
  std::unique_ptr<views::Label> main_text_label =
      CreateMainTextLabel(suggestion, show_new_badge);
  main_text_label->SetMaximumWidthSingleLine(kAutofillPopupUsernameMaxWidth);
  if (filter_match) {
    main_text_label->SetTextStyleRange(kMainTextStyleHighlighted,
                                       filter_match->main_text_match);
  }

  std::vector<std::unique_ptr<views::View>> subtext_views;
  subtext_views.push_back(CreatePasswordSubtextView(suggestion));
  popup_cell_utils::AddSuggestionContentToView(
      suggestion, std::move(main_text_label), CreateMinorTextLabel(suggestion),
      CreatePasswordDescriptionLabel(suggestion), std::move(subtext_views),
      GetPasswordIconView(suggestion, favicon_loader), *view);

  return view;
}

std::unique_ptr<PopupRowContentView> CreateComposePopupRowContentView(
    const Suggestion& suggestion,
    std::optional<user_education::DisplayNewBadge> show_new_badge) {
  auto view = std::make_unique<PopupRowContentView>();
  auto main_text_label = std::make_unique<user_education::NewBadgeLabel>(
      suggestion.main_text.value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_BODY_3_MEDIUM);
  if (show_new_badge.has_value()) {
    main_text_label->SetDisplayNewBadge(show_new_badge.value());
    main_text_label->SetPadAfterNewBadge(false);
  }
  popup_cell_utils::AddSuggestionContentToView(
      suggestion, std::move(main_text_label),
      /*minor_text_label=*/nullptr,
      /*description_label=*/nullptr, /*subtext_views=*/
      CreateSubtextViews(*view, suggestion, FillingProduct::kCompose),
      popup_cell_utils::GetIconImageView(suggestion), *view);

  return view;
}

// Creates the content view for regular address and credit card suggestions.
// Content views for suggestions of other types and special suggestions are
// created by corresponding `Create*PopupRowContentView()` methods.
std::unique_ptr<PopupRowContentView> CreatePopupRowContentView(
    const Suggestion& suggestion,
    std::optional<user_education::DisplayNewBadge> show_new_badge,
    FillingProduct main_filling_product,
    std::optional<AutofillPopupController::SuggestionFilterMatch>
        filter_match) {
  auto view = std::make_unique<PopupRowContentView>();
  std::unique_ptr<views::Label> main_text_label =
      CreateMainTextLabel(suggestion, show_new_badge);
  if (filter_match) {
    main_text_label->SetTextStyleRange(kMainTextStyleHighlighted,
                                       filter_match->main_text_match);
  }

  FormatLabel(*main_text_label, suggestion.main_text, main_filling_product,
              GetMaxPopupAddressProfileWidth(ShouldApplyNewPopupMaxWidth(
                  suggestion.type, suggestion.is_acceptable)));
  popup_cell_utils::AddSuggestionContentToView(
      suggestion, std::move(main_text_label), CreateMinorTextLabel(suggestion),
      /*description_label=*/nullptr,
      CreateSubtextViews(*view, suggestion, main_filling_product),
      popup_cell_utils::GetIconImageView(suggestion), *view);
  return view;
}

// Creates the row for an Autocomplete entry with a delete button.
std::unique_ptr<PopupRowWithButtonView> CreateAutocompleteRowWithDeleteButton(
    base::WeakPtr<AutofillPopupController> controller,
    PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
    PopupRowView::SelectionDelegate& selection_delegate,
    int line_number) {
  auto view = std::make_unique<PopupRowContentView>();

  const Suggestion& kSuggestion = controller->GetSuggestionAt(line_number);
  std::unique_ptr<views::Label> main_text_label =
      CreateMainTextLabel(kSuggestion, /*show_new_badge=*/std::nullopt);
  FormatLabel(*main_text_label, kSuggestion.main_text,
              FillingProduct::kAutocomplete,
              GetMaxPopupAddressProfileWidth(ShouldApplyNewPopupMaxWidth(
                  kSuggestion.type, kSuggestion.is_acceptable)));
  popup_cell_utils::AddSuggestionContentToView(
      kSuggestion, std::move(main_text_label),
      CreateMinorTextLabel(kSuggestion),
      /*description_label=*/nullptr,
      CreateSubtextViews(*view, kSuggestion, FillingProduct::kAutocomplete),
      popup_cell_utils::GetIconImageView(kSuggestion), *view);

  // The closure that actually attempts to delete an entry and record metrics
  // for it.
  base::RepeatingClosure deletion_action = base::BindRepeating(
      base::IgnoreResult(&AutofillPopupController::RemoveSuggestion),
      controller, line_number,
      AutofillMetrics::SingleEntryRemovalMethod::kDeleteButtonClicked);
  std::unique_ptr<views::ImageButton> button =
      views::CreateVectorImageButtonWithNativeTheme(
          CreateExecuteSoonWrapper(std::move(deletion_action)),
          views::kIcCloseIcon, kCloseIconSize);

  // We are making sure that the vertical distance from the delete button edges
  // to the cell border is the same as the horizontal distance.
  // 1. Take the current horizontal distance.
  int horizontal_margin = view->GetInsideBorderInsets().right();
  // 2. Take the height of the cell.
  int cell_height = view->GetMinimumCrossAxisSize();
  // 3. The diameter needs to be the height - 2 * the desired margin.
  int radius = (cell_height - horizontal_margin * 2) / 2;
  views::InstallFixedSizeCircleHighlightPathGenerator(button.get(), radius);
  button->SetPreferredSize(gfx::Size(radius * 2, radius * 2));
  button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_TOOLTIP));
  button->GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);
  button->GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_A11Y_HINT,
      popup_cell_utils::GetVoiceOverStringFromSuggestion(
          controller->GetSuggestionAt(line_number))));
  button->SetVisible(false);

  return std::make_unique<PopupRowWithButtonView>(
      a11y_selection_delegate, selection_delegate, controller, line_number,
      std::move(view), std::move(button),
      PopupRowWithButtonView::ButtonVisibility::kShowOnHoverOrSelect,
      PopupRowWithButtonView::ButtonSelectBehavior::kUnselectSuggestion);
}

// Creates the row for creating a plus address inline.
// TODO(crbug.com/362445807): Add pixel tests once the layout is complete.
std::unique_ptr<PopupRowView> CreateNewPlusAddressInlineSuggestion(
    base::WeakPtr<AutofillPopupController> controller,
    PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
    PopupRowView::SelectionDelegate& selection_delegate,
    int line_number,
    std::optional<user_education::DisplayNewBadge> show_new_badge) {
  auto view = std::make_unique<PopupRowContentView>();

  const Suggestion& kSuggestion = controller->GetSuggestionAt(line_number);
  std::unique_ptr<views::Label> main_text_label =
      CreateMainTextLabel(kSuggestion, show_new_badge);
  FormatLabel(*main_text_label, kSuggestion.main_text,
              FillingProduct::kPlusAddresses,
              GetMaxPopupAddressProfileWidth(ShouldApplyNewPopupMaxWidth(
                  kSuggestion.type, kSuggestion.is_acceptable)));
  popup_cell_utils::AddSuggestionContentToView(
      kSuggestion, std::move(main_text_label),
      CreateMinorTextLabel(kSuggestion),
      /*description_label=*/nullptr,
      CreateSubtextViews(*view, kSuggestion, FillingProduct::kPlusAddresses),
      popup_cell_utils::GetIconImageView(kSuggestion), *view);

  // If no refresh is offered, we can just use a "normal" `PopupRowView`.
  if (!kSuggestion.GetPayload<Suggestion::PlusAddressPayload>().offer_refresh) {
    return std::make_unique<PopupRowView>(a11y_selection_delegate,
                                          selection_delegate, controller,
                                          line_number, std::move(view));
  }

  base::RepeatingClosure action = base::BindRepeating(
      &AutofillPopupController::PerformButtonActionForSuggestion, controller,
      line_number, SuggestionButtonAction());
  std::unique_ptr<views::ImageButton> button =
      views::CreateVectorImageButtonWithNativeTheme(
          CreateExecuteSoonWrapper(std::move(action)),
          vector_icons::kReloadIcon, kRefreshIconSize);

  button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_PLUS_ADDRESS_CREATE_INLINE_REFRESH_TOOLTIP));
  button->GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);
  button->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_PLUS_ADDRESS_CREATE_INLINE_REFRESH_A11Y_NAME));
  button->SetVisible(false);
  views::InstallFixedSizeCircleHighlightPathGenerator(button.get(),
                                                      kRefreshInkDropRadius);

  return std::make_unique<PopupRowWithButtonView>(
      a11y_selection_delegate, selection_delegate, controller, line_number,
      std::move(view), std::move(button),
      PopupRowWithButtonView::ButtonVisibility::kShowOnHoverOrSelect,
      PopupRowWithButtonView::ButtonSelectBehavior::kSelectSuggestion);
}

// Creates the row for the `SuggestionType::kPredictionImprovementsFeedback`
// suggestion.
std::unique_ptr<PopupRowPredictionImprovementsFeedbackView>
CreatePredictionImprovementsFeedbackRow(
    base::WeakPtr<AutofillPopupController> controller,
    PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
    PopupRowView::SelectionDelegate& selection_delegate,
    int line_number) {
  return std::make_unique<PopupRowPredictionImprovementsFeedbackView>(
      a11y_selection_delegate, selection_delegate, controller, line_number);
}

}  // namespace

std::unique_ptr<PopupRowView> CreatePopupRowView(
    base::WeakPtr<AutofillPopupController> controller,
    PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
    PopupRowView::SelectionDelegate& selection_delegate,
    int line_number,
    std::optional<AutofillPopupController::SuggestionFilterMatch> filter_match,
    PasswordFaviconLoader* favicon_loader) {
  CHECK(controller);

  const Suggestion& suggestion = controller->GetSuggestionAt(line_number);
  SuggestionType type = suggestion.type;
  FillingProduct main_filling_product = controller->GetMainFillingProduct();

  if (type == SuggestionType::kAutocompleteEntry) {
    return CreateAutocompleteRowWithDeleteButton(
        controller, a11y_selection_delegate, selection_delegate, line_number);
  }

  if (type == SuggestionType::kPredictionImprovementsFeedback) {
    return CreatePredictionImprovementsFeedbackRow(
        controller, a11y_selection_delegate, selection_delegate, line_number);
  }

  if (IsFooterSuggestionType(type)) {
    return std::make_unique<PopupRowView>(
        a11y_selection_delegate, selection_delegate, controller, line_number,
        CreateFooterPopupRowContentView(suggestion));
  }

  const auto show_new_badge =
      suggestion.feature_for_new_badge
          ? std::optional<user_education::DisplayNewBadge>(
                UserEducationService::MaybeShowNewBadge(
                    controller->GetWebContents()->GetBrowserContext(),
                    *suggestion.feature_for_new_badge))
          : std::nullopt;

  switch (type) {
    // These `type` should never be displayed in a `PopupRowView`.
    case SuggestionType::kSeparator:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
      NOTREACHED();
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kAccountStoragePasswordEntry:
      return std::make_unique<PopupRowView>(
          a11y_selection_delegate, selection_delegate, controller, line_number,
          CreatePasswordPopupRowContentView(suggestion, show_new_badge,
                                            std::move(filter_match),
                                            favicon_loader));
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification: {
      return std::make_unique<PopupRowView>(
          a11y_selection_delegate, selection_delegate, controller, line_number,
          CreateComposePopupRowContentView(suggestion, show_new_badge));
    }
    case SuggestionType::kComposeProactiveNudge: {
      return std::make_unique<PopupRowView>(
          a11y_selection_delegate, selection_delegate, controller, line_number,
          CreateComposePopupRowContentView(suggestion, show_new_badge));
    }
    case SuggestionType::kCreateNewPlusAddressInline:
    case SuggestionType::kPlusAddressError: {
      return CreateNewPlusAddressInlineSuggestion(
          controller, a11y_selection_delegate, selection_delegate, line_number,
          show_new_badge);
    }
    default:
      return std::make_unique<PopupRowView>(
          a11y_selection_delegate, selection_delegate, controller, line_number,
          CreatePopupRowContentView(suggestion, show_new_badge,
                                    main_filling_product,
                                    std::move(filter_match)));
  }
}

}  // namespace autofill
