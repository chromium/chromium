// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"

#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_with_button_view.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
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
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace autofill {

namespace {

// The size of a close or delete icon.
constexpr int kCloseIconSize = 16;

// Popup items that use a leading icon instead of a trailing one.
constexpr auto kPopupItemTypesUsingLeadingIcons =
    base::MakeFixedFlatSet<PopupItemId>(
        {PopupItemId::kClearForm, PopupItemId::kShowAccountCards,
         PopupItemId::kAutofillOptions, PopupItemId::kEditAddressProfile,
         PopupItemId::kDeleteAddressProfile,
         PopupItemId::kAllSavedPasswordsEntry,
         PopupItemId::kFillEverythingFromAddressProfile,
         PopupItemId::kPasswordAccountStorageEmpty,
         PopupItemId::kPasswordAccountStorageOptIn,
         PopupItemId::kPasswordAccountStorageReSignin,
         PopupItemId::kPasswordAccountStorageOptInAndGenerate});

// Max width for the username and masked password.
constexpr int kAutofillPopupUsernameMaxWidth = 272;
constexpr int kAutofillPopupPasswordMaxWidth = 108;

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

std::unique_ptr<PopupRowContentView> CreateFooterPopupRowContentView(
    const Suggestion& suggestion) {
  auto view = std::make_unique<PopupRowContentView>();
  views::BoxLayout* layout_manager =
      view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          popup_cell_utils::GetMarginsForContentCell(
              /*has_control_element=*/false)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  std::unique_ptr<views::ImageView> icon =
      popup_cell_utils::GetIconImageView(suggestion);

  const bool kUseLeadingIcon =
      kPopupItemTypesUsingLeadingIcons.contains(suggestion.popup_item_id);

  if (suggestion.is_loading) {
    view->AddChildView(std::make_unique<views::Throbber>())->Start();
    popup_cell_utils::AddSpacerWithSize(*view, *layout_manager,
                                        PopupBaseView::GetHorizontalPadding(),
                                        /*resize=*/false);
  } else if (icon && kUseLeadingIcon) {
    view->AddChildView(std::move(icon));
    popup_cell_utils::AddSpacerWithSize(*view, *layout_manager,
                                        PopupBaseView::GetHorizontalPadding(),
                                        /*resize=*/false);
  }

  layout_manager->set_minimum_cross_axis_size(
      views::MenuConfig::instance().touchable_menu_height);

  std::unique_ptr<views::Label> main_text_label =
      popup_cell_utils::CreateMainTextLabel(suggestion.main_text,
                                            views::style::STYLE_SECONDARY);
  main_text_label->SetEnabled(!suggestion.is_loading);
  view->TrackLabel(view->AddChildView(std::move(main_text_label)));

  popup_cell_utils::AddSpacerWithSize(*view, *layout_manager,
                                      /*spacer_width=*/0,
                                      /*resize=*/true);

  if (icon && !kUseLeadingIcon) {
    popup_cell_utils::AddSpacerWithSize(*view, *layout_manager,
                                        PopupBaseView::GetHorizontalPadding(),
                                        /*resize=*/false);
    view->AddChildView(std::move(icon));
  }

  std::unique_ptr<views::ImageView> trailing_icon =
      popup_cell_utils::GetTrailingIconImageView(suggestion);
  if (trailing_icon) {
    popup_cell_utils::AddSpacerWithSize(*view, *layout_manager,
                                        PopupBaseView::GetHorizontalPadding(),
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
  content_view.TrackLabel(label.get());
  std::vector<std::unique_ptr<views::View>> result;
  result.push_back(std::move(label));
  return result;
}

std::unique_ptr<PopupRowContentView> CreatePasswordPopupRowContentView(
    const Suggestion& suggestion) {
  auto view = std::make_unique<PopupRowContentView>();

  // Add the actual views.
  std::unique_ptr<views::Label> main_text_label =
      popup_cell_utils::CreateMainTextLabel(
          suggestion.main_text, views::style::TextStyle::STYLE_PRIMARY);
  main_text_label->SetMaximumWidthSingleLine(kAutofillPopupUsernameMaxWidth);

  popup_cell_utils::AddSuggestionContentToView(
      suggestion, std::move(main_text_label),
      popup_cell_utils::CreateMinorTextLabel(suggestion.minor_text),
      CreatePasswordDescriptionLabel(suggestion),
      CreateAndTrackPasswordSubtextViews(suggestion, *view), *view);

  return view;
}

std::unique_ptr<PopupRowContentView> CreateComposePopupRowContentView(
    const Suggestion& suggestion,
    PopupType popup_type,
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
      popup_cell_utils::CreateAndTrackSubtextViews(
          *view, suggestion, popup_type, views::style::STYLE_BODY_4),
      *view);

  return view;
}

// Creates the content view for regular address and credit card suggestions.
// Content views for suggestions of other types and special suggestions are
// created by corresponding `Create*PopupRowContentView()` methods.
std::unique_ptr<PopupRowContentView> CreatePopupRowContentView(
    const Suggestion& suggestion,
    PopupType popup_type) {
  auto view = std::make_unique<PopupRowContentView>();
  std::unique_ptr<views::Label> main_text_label =
      popup_cell_utils::CreateMainTextLabel(
          suggestion.main_text,
          GetMainTextStyleForPopupItemId(suggestion.popup_item_id));
  popup_cell_utils::FormatLabel(*main_text_label, suggestion.main_text,
                                popup_type);
  popup_cell_utils::AddSuggestionContentToView(
      suggestion, std::move(main_text_label),
      popup_cell_utils::CreateMinorTextLabel(suggestion.minor_text),
      /*description_label=*/nullptr,
      popup_cell_utils::CreateAndTrackSubtextViews(*view, suggestion,
                                                   popup_type),
      *view);
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
      popup_cell_utils::CreateMainTextLabel(
          kSuggestion.main_text,
          GetMainTextStyleForPopupItemId(kSuggestion.popup_item_id));
  popup_cell_utils::FormatLabel(*main_text_label, kSuggestion.main_text,
                                controller->GetPopupType());
  popup_cell_utils::AddSuggestionContentToView(
      kSuggestion, std::move(main_text_label),
      popup_cell_utils::CreateMinorTextLabel(kSuggestion.minor_text),
      /*description_label=*/nullptr,
      popup_cell_utils::CreateAndTrackSubtextViews(*view, kSuggestion,
                                                   controller->GetPopupType()),
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
      [](base::WeakPtr<AutofillPopupController> controller, int line_number) {
        if (controller && controller->RemoveSuggestion(line_number)) {
          AutofillMetrics::OnAutocompleteSuggestionDeleted(
              AutofillMetrics::AutocompleteSingleEntryRemovalMethod::
                  kDeleteButtonClicked);
        }
      },
      controller, line_number);
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
    int line_number) {
  CHECK(controller);

  const Suggestion& suggestion = controller->GetSuggestionAt(line_number);
  PopupItemId popup_item_id = suggestion.popup_item_id;
  PopupType popup_type = controller->GetPopupType();

  if (popup_item_id == PopupItemId::kAutocompleteEntry &&
      base::FeatureList::IsEnabled(
          features::kAutofillShowAutocompleteDeleteButton)) {
    return CreateAutocompleteRowWithDeleteButton(
        controller, a11y_selection_delegate, selection_delegate, line_number);
  }

  if (IsFooterPopupItemId(popup_item_id)) {
    return std::make_unique<PopupRowView>(
        a11y_selection_delegate, selection_delegate, controller, line_number,
        CreateFooterPopupRowContentView(suggestion));
  }

  switch (popup_item_id) {
    // These `popup_item_id` should never be displayed in a `PopupRowView`.
    case PopupItemId::kSeparator:
    case PopupItemId::kMixedFormMessage:
    case PopupItemId::kInsecureContextPaymentDisabledMessage:
      NOTREACHED_NORETURN();
    case PopupItemId::kUsernameEntry:
    case PopupItemId::kPasswordEntry:
    case PopupItemId::kAccountStorageUsernameEntry:
    case PopupItemId::kAccountStoragePasswordEntry:
      return std::make_unique<PopupRowView>(
          a11y_selection_delegate, selection_delegate, controller, line_number,
          CreatePasswordPopupRowContentView(suggestion));
    case PopupItemId::kCompose: {
      auto tracker = std::make_unique<ScopedNewBadgeTracker>(
          controller->GetWebContents()->GetBrowserContext());
      const bool show_new_badge = tracker->TryShowNewBadge(
          feature_engagement::kIPHComposeNewBadgeFeature,
          &compose::features::kEnableCompose);
      auto new_badge_tracker =
          PopupRowView::ScopedNewBadgeTrackerWithAcceptAction(
              std::move(tracker),
              /*action_name=*/"compose_activated");
      auto row_view = std::make_unique<PopupRowView>(
          a11y_selection_delegate, selection_delegate, controller, line_number,
          CreateComposePopupRowContentView(suggestion, popup_type,
                                           show_new_badge));
      row_view->set_new_badge_tracker(std::move(new_badge_tracker));
      return row_view;
    };
    default:
      return std::make_unique<PopupRowView>(
          a11y_selection_delegate, selection_delegate, controller, line_number,
          CreatePopupRowContentView(suggestion, popup_type));
  }

  NOTREACHED_NORETURN();
}

}  // namespace autofill
