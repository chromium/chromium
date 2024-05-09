// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_with_button_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/views/new_badge_label.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace autofill {

namespace {

// The size of a close or delete icon.
constexpr int kCloseIconSize = 16;

// Popup items that use a leading icon instead of a trailing one.
constexpr auto kPopupItemTypesUsingLeadingIcons =
    base::MakeFixedFlatSet<SuggestionType>(
        {SuggestionType::kClearForm, SuggestionType::kShowAccountCards,
         SuggestionType::kAutofillOptions, SuggestionType::kEditAddressProfile,
         SuggestionType::kDeleteAddressProfile,
         SuggestionType::kAllSavedPasswordsEntry,
         SuggestionType::kFillEverythingFromAddressProfile,
         SuggestionType::kPasswordAccountStorageEmpty,
         SuggestionType::kPasswordAccountStorageOptIn,
         SuggestionType::kPasswordAccountStorageReSignin,
         SuggestionType::kPasswordAccountStorageOptInAndGenerate,
         SuggestionType::kViewPasswordDetails});

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
    case FillingProduct::kNone:
      break;
  }
}

int GetMaxPopupAddressProfileWidth() {
  // TODO(crbug.com/40274514): Remove feature check as part of the clean up.
  return base::FeatureList::IsEnabled(
             features::kAutofillGranularFillingAvailable)
             ? kAutofillPopupAddressProfileGranularFillingEnabledMaxWidth
             : kAutofillSuggestionMaxWidth;
}

// Creates a label for the suggestion's main text.
std::unique_ptr<views::Label> CreateMainTextLabel(
    const Suggestion& suggestion,
    views::style::TextStyle primary_text_style = kMainTextStyle) {
  auto label = std::make_unique<views::Label>(
      suggestion.main_text.value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      suggestion.main_text.is_primary ? primary_text_style
                                      : kMainTextStyleLight);

  if (!suggestion.main_text.is_primary) {
    label->SetEnabledColorId(ui::kColorLabelForegroundSecondary);
  }
  if (suggestion.apply_deactivated_style) {
    popup_cell_utils::ApplyDeactivatedStyle(*label);
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
      kMinorTextStyle);
  label->SetEnabledColorId(ui::kColorLabelForegroundSecondary);
  if (suggestion.apply_deactivated_style) {
    popup_cell_utils::ApplyDeactivatedStyle(*label);
  }
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
              kMinorTextStyle));
      label->SetEnabledColorId(ui::kColorLabelForegroundSecondary);
      // TODO(crbug.com/40274514): Remove feature check as part of the clean up.
      if (!base::FeatureList::IsEnabled(
              features::kAutofillGranularFillingAvailable)) {
        FormatLabel(*label, label_text, main_filling_product,
                    GetMaxPopupAddressProfileWidth());
      } else {
        // To make sure the popup width will not exceed its maximum value,
        // divide the maximum label width by the number of labels.
        FormatLabel(*label, label_text, main_filling_product,
                    GetMaxPopupAddressProfileWidth() / label_row.size());
      }
    }
    result.push_back(std::move(label_row_container_view));
  }

  return result;
}

std::unique_ptr<PopupRowContentView> CreateFooterPopupRowContentView(
    const Suggestion& suggestion) {
  auto view = std::make_unique<PopupRowContentView>();
  views::BoxLayout* layout_manager =
      view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          popup_cell_utils::GetMarginsForContentCell()));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  std::unique_ptr<views::ImageView> icon =
      popup_cell_utils::GetIconImageView(suggestion);

  const bool kUseLeadingIcon =
      kPopupItemTypesUsingLeadingIcons.contains(suggestion.type);

  if (suggestion.is_loading) {
    view->AddChildView(std::make_unique<views::Throbber>())->Start();
    popup_cell_utils::AddSpacerWithSize(*view, *layout_manager,
                                        PopupBaseView::ArrowHorizontalMargin(),
                                        /*resize=*/false);
  } else if (icon && kUseLeadingIcon) {
    view->AddChildView(std::move(icon));
    popup_cell_utils::AddSpacerWithSize(*view, *layout_manager,
                                        PopupBaseView::ArrowHorizontalMargin(),
                                        /*resize=*/false);
  }

  layout_manager->set_minimum_cross_axis_size(
      views::MenuConfig::instance().touchable_menu_height);

  std::unique_ptr<views::Label> main_text_label =
      CreateMainTextLabel(suggestion, kMainTextStyleLight);
  main_text_label->SetEnabledColorId(ui::kColorLabelForegroundSecondary);
  main_text_label->SetEnabled(!suggestion.is_loading);
  view->AddChildView(std::move(main_text_label));

  popup_cell_utils::AddSpacerWithSize(*view, *layout_manager,
                                      /*spacer_width=*/0,
                                      /*resize=*/true);

  if (icon && !kUseLeadingIcon) {
    popup_cell_utils::AddSpacerWithSize(*view, *layout_manager,
                                        PopupBaseView::ArrowHorizontalMargin(),
                                        /*resize=*/false);
    view->AddChildView(std::move(icon));
  }

  std::unique_ptr<views::ImageView> trailing_icon =
      popup_cell_utils::GetTrailingIconImageView(suggestion);
  if (trailing_icon) {
    popup_cell_utils::AddSpacerWithSize(*view, *layout_manager,
                                        PopupBaseView::ArrowHorizontalMargin(),
                                        /*resize=*/true);
    view->AddChildView(std::move(trailing_icon));
  }

  // Force a refresh to ensure all the labels'styles are correct.
  view->UpdateStyle(/*selected=*/false);

  return view;
}

std::unique_ptr<views::Label> CreatePasswordDescriptionLabel(
    const Suggestion& suggestion) {
  if (suggestion.labels.empty()) {
    return nullptr;
  }

  CHECK_EQ(suggestion.labels.size(), 1u);
  CHECK_EQ(suggestion.labels[0].size(), 1u);

  auto label = std::make_unique<views::Label>(
      suggestion.labels[0][0].value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  label->SetElideBehavior(gfx::ELIDE_HEAD);
  label->SetMaximumWidthSingleLine(kAutofillPopupUsernameMaxWidth);
  return label;
}

std::vector<std::unique_ptr<views::View>> CreateAndTrackPasswordSubtextViews(
    const Suggestion& suggestion,
    PopupRowContentView& content_view) {
  auto label = std::make_unique<views::Label>(
      suggestion.additional_label, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  label->SetElideBehavior(gfx::TRUNCATE);
  label->SetMaximumWidthSingleLine(kAutofillPopupPasswordMaxWidth);
  std::vector<std::unique_ptr<views::View>> result;
  result.push_back(std::move(label));
  return result;
}

std::unique_ptr<PopupRowContentView> CreatePasswordPopupRowContentView(
    const Suggestion& suggestion,
    std::optional<AutofillPopupController::SuggestionFilterMatch>
        filter_match) {
  auto view = std::make_unique<PopupRowContentView>();

  // Add the actual views.
  std::unique_ptr<views::Label> main_text_label =
      CreateMainTextLabel(suggestion);
  main_text_label->SetMaximumWidthSingleLine(kAutofillPopupUsernameMaxWidth);
  if (filter_match) {
    main_text_label->SetTextStyleRange(kMainTextStyleHighlighted,
                                       filter_match->main_text_match);
  }

  popup_cell_utils::AddSuggestionContentToView(
      suggestion, std::move(main_text_label), CreateMinorTextLabel(suggestion),
      CreatePasswordDescriptionLabel(suggestion),
      CreateAndTrackPasswordSubtextViews(suggestion, *view), *view);

  return view;
}

std::unique_ptr<PopupRowContentView> CreateComposePopupRowContentView(
    const Suggestion& suggestion,
    bool show_new_badge) {
  auto view = std::make_unique<PopupRowContentView>();
  auto main_text_label = std::make_unique<user_education::NewBadgeLabel>(
      suggestion.main_text.value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_BODY_3_MEDIUM);
  main_text_label->SetDisplayNewBadge(show_new_badge);
  popup_cell_utils::AddSuggestionContentToView(
      suggestion, std::move(main_text_label),
      /*minor_text_label=*/nullptr,
      /*description_label=*/nullptr, /*subtext_views=*/
      CreateSubtextViews(*view, suggestion, FillingProduct::kCompose), *view);

  return view;
}

// Creates the content view for regular address and credit card suggestions.
// Content views for suggestions of other types and special suggestions are
// created by corresponding `Create*PopupRowContentView()` methods.
std::unique_ptr<PopupRowContentView> CreatePopupRowContentView(
    const Suggestion& suggestion,
    FillingProduct main_filling_product,
    std::optional<AutofillPopupController::SuggestionFilterMatch>
        filter_match) {
  auto view = std::make_unique<PopupRowContentView>();
  std::unique_ptr<views::Label> main_text_label =
      CreateMainTextLabel(suggestion);
  if (filter_match) {
    main_text_label->SetTextStyleRange(kMainTextStyleHighlighted,
                                       filter_match->main_text_match);
  }

  FormatLabel(*main_text_label, suggestion.main_text, main_filling_product,
              GetMaxPopupAddressProfileWidth());
  popup_cell_utils::AddSuggestionContentToView(
      suggestion, std::move(main_text_label), CreateMinorTextLabel(suggestion),
      /*description_label=*/nullptr,
      CreateSubtextViews(*view, suggestion, main_filling_product), *view);
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
      CreateMainTextLabel(kSuggestion);
  FormatLabel(*main_text_label, kSuggestion.main_text,
              controller->GetMainFillingProduct(),
              GetMaxPopupAddressProfileWidth());
  popup_cell_utils::AddSuggestionContentToView(
      kSuggestion, std::move(main_text_label),
      CreateMinorTextLabel(kSuggestion),
      /*description_label=*/nullptr,
      CreateSubtextViews(*view, kSuggestion,
                         controller->GetMainFillingProduct()),
      *view);

  // Setup a layout of the delete button for Autocomplete entries.
  views::BoxLayout* layout =
      static_cast<views::BoxLayout*>(view->GetLayoutManager());
  for (views::View* child : view->children()) {
    layout->SetFlexForView(child, 1);
  }

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
  int horizontal_margin = layout->inside_border_insets().right();
  // 2. Take the height of the cell.
  int cell_height = layout->minimum_cross_axis_size();
  // 3. The diameter needs to be the height - 2 * the desired margin.
  int radius = (cell_height - horizontal_margin * 2) / 2;
  views::InstallFixedSizeCircleHighlightPathGenerator(button.get(), radius);
  button->SetPreferredSize(gfx::Size(radius * 2, radius * 2));
  button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_TOOLTIP));
  button->SetAccessibleRole(ax::mojom::Role::kMenuItem);
  button->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_A11Y_HINT,
      popup_cell_utils::GetVoiceOverStringFromSuggestion(
          controller->GetSuggestionAt(line_number))));
  button->SetVisible(false);

  return std::make_unique<PopupRowWithButtonView>(
      a11y_selection_delegate, selection_delegate, controller, line_number,
      std::move(view), std::move(button),
      PopupRowWithButtonView::ButtonBehavior::kShowOnHoverOrSelect);
}

}  // namespace

std::unique_ptr<PopupRowView> CreatePopupRowView(
    base::WeakPtr<AutofillPopupController> controller,
    PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
    PopupRowView::SelectionDelegate& selection_delegate,
    int line_number,
    std::optional<AutofillPopupController::SuggestionFilterMatch>
        filter_match) {
  CHECK(controller);

  const Suggestion& suggestion = controller->GetSuggestionAt(line_number);
  SuggestionType type = suggestion.type;
  FillingProduct main_filling_product = controller->GetMainFillingProduct();

  if (type == SuggestionType::kAutocompleteEntry) {
    return CreateAutocompleteRowWithDeleteButton(
        controller, a11y_selection_delegate, selection_delegate, line_number);
  }

  if (IsFooterSuggestionType(type)) {
    return std::make_unique<PopupRowView>(
        a11y_selection_delegate, selection_delegate, controller, line_number,
        CreateFooterPopupRowContentView(suggestion));
  }

  switch (type) {
    // These `type` should never be displayed in a `PopupRowView`.
    case SuggestionType::kSeparator:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
      NOTREACHED_NORETURN();
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kAccountStoragePasswordEntry:
      return std::make_unique<PopupRowView>(
          a11y_selection_delegate, selection_delegate, controller, line_number,
          CreatePasswordPopupRowContentView(suggestion,
                                            std::move(filter_match)));
    case SuggestionType::kCompose: {
      const bool show_new_badge = UserEducationService::MaybeShowNewBadge(
          controller->GetWebContents()->GetBrowserContext(),
          compose::features::kEnableComposeSavedStateNudge);
      return std::make_unique<PopupRowView>(
          a11y_selection_delegate, selection_delegate, controller, line_number,
          CreateComposePopupRowContentView(suggestion, show_new_badge));
    }
    default:
      return std::make_unique<PopupRowView>(
          a11y_selection_delegate, selection_delegate, controller, line_number,
          CreatePopupRowContentView(suggestion, main_filling_product,
                                    std::move(filter_match)));
  }

  NOTREACHED_NORETURN();
}

}  // namespace autofill
