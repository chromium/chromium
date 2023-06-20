// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_strategy.h"

#include "base/feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_autocomplete_cell_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout_view.h"

namespace autofill {

namespace {

// Max width for the username and masked password.
constexpr int kAutofillPopupUsernameMaxWidth = 272;
constexpr int kAutofillPopupPasswordMaxWidth = 108;

// Popup items that use a leading icon instead of a trailing one.
constexpr PopupItemId kItemTypesUsingLeadingIcons[] = {
    PopupItemId::kClearForm,
    PopupItemId::kShowAccountCards,
    PopupItemId::kAutofillOptions,
    PopupItemId::kAllSavedPasswordsEntry,
    PopupItemId::kPasswordAccountStorageEmpty,
    PopupItemId::kPasswordAccountStorageOptIn,
    PopupItemId::kPasswordAccountStorageReSignin,
    PopupItemId::kPasswordAccountStorageOptInAndGenerate};

// ********************* AccessibilityDelegate implementations *****************

// ********************* ContentItemAccessibilityDelegate  *********************
class ContentItemAccessibilityDelegate
    : public PopupCellView::AccessibilityDelegate {
 public:
  // Creates an a11y delegate for the `line_number`. `controller` must not be
  // null.
  ContentItemAccessibilityDelegate(
      base::WeakPtr<AutofillPopupController> controller,
      int line_number);
  ~ContentItemAccessibilityDelegate() override = default;

  void GetAccessibleNodeData(bool is_selected,
                             ui::AXNodeData* node_data) const override;

 private:
  // The string announced via VoiceOver.
  std::u16string voice_over_string_;
  // The number of suggestions in the popup and the (1-based) index of the
  // suggestion this delegate belongs to.
  int set_index_ = 0;
  int set_size_ = 0;
};

ContentItemAccessibilityDelegate::ContentItemAccessibilityDelegate(
    base::WeakPtr<AutofillPopupController> controller,
    int line_number) {
  DCHECK(controller);

  voice_over_string_ = popup_cell_utils::GetVoiceOverStringFromSuggestion(
      controller->GetSuggestionAt(line_number));

  set_size_ = 0;
  set_index_ = line_number + 1;
  for (int i = 0; i < controller->GetLineCount(); ++i) {
    if (controller->GetSuggestionAt(i).popup_item_id ==
        PopupItemId::kSeparator) {
      if (i < line_number) {
        --set_index_;
      }
    } else {
      ++set_size_;
    }
  }
}

void ContentItemAccessibilityDelegate::GetAccessibleNodeData(
    bool is_selected,
    ui::AXNodeData* node_data) const {
  DCHECK(node_data);
  // Options are selectable.
  node_data->role = ax::mojom::Role::kListBoxOption;
  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, is_selected);
  node_data->SetNameChecked(voice_over_string_);

  node_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, set_index_);
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize, set_size_);
}

// ******************** DeleteButtonAccessibilityDelegate  *********************
class DeleteButtonAccessibilityDelegate
    : public PopupCellView::AccessibilityDelegate {
 public:
  DeleteButtonAccessibilityDelegate(
      base::WeakPtr<AutofillPopupController> controller,
      int line_number);
  ~DeleteButtonAccessibilityDelegate() override = default;

  void GetAccessibleNodeData(bool is_selected,
                             ui::AXNodeData* node_data) const override;

 private:
  std::u16string voice_over_string_;
};

DeleteButtonAccessibilityDelegate::DeleteButtonAccessibilityDelegate(
    base::WeakPtr<AutofillPopupController> controller,
    int line_number) {
  DCHECK(controller);
  voice_over_string_ = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_A11Y_HINT,
      popup_cell_utils::GetVoiceOverStringFromSuggestion(
          controller->GetSuggestionAt(line_number)));
}

void DeleteButtonAccessibilityDelegate::GetAccessibleNodeData(
    bool is_selected,
    ui::AXNodeData* node_data) const {
  node_data->role = ax::mojom::Role::kMenuItem;
  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, is_selected);
  node_data->SetNameChecked(voice_over_string_);
}

}  // namespace

/**************************** PopupRowBaseStrategy ****************************/

PopupRowBaseStrategy::PopupRowBaseStrategy(
    base::WeakPtr<AutofillPopupController> controller,
    int line_number)
    : controller_(controller), line_number_(line_number) {
  DCHECK(controller_);
}

PopupRowBaseStrategy::~PopupRowBaseStrategy() = default;

int PopupRowBaseStrategy::GetLineNumber() const {
  return line_number_;
}

/************************** PopupSuggestionStrategy ***************************/

PopupSuggestionStrategy::PopupSuggestionStrategy(
    base::WeakPtr<AutofillPopupController> controller,
    int line_number)
    : PopupRowBaseStrategy(std::move(controller), line_number),
      popup_type_(GetController()->GetPopupType()) {}

PopupSuggestionStrategy::~PopupSuggestionStrategy() = default;

std::unique_ptr<PopupCellView> PopupSuggestionStrategy::CreateContent() {
  if (!GetController()) {
    return nullptr;
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillShowAutocompleteDeleteButton) &&
      GetController()->GetSuggestionAt(GetLineNumber()).popup_item_id ==
          PopupItemId::kAutocompleteEntry) {
    return CreateDeleteAutocompleteRow();
  }
  const Suggestion& kSuggestion =
      GetController()->GetSuggestionAt(GetLineNumber());
  std::unique_ptr<PopupCellView> view =
      views::Builder<PopupCellView>()
          .SetAccessibilityDelegate(
              std::make_unique<ContentItemAccessibilityDelegate>(
                  GetController(), GetLineNumber()))
          .Build();

  // Add the actual views.
  std::unique_ptr<views::Label> main_text_label =
      popup_cell_utils::CreateMainTextLabel(
          kSuggestion.main_text, views::style::TextStyle::STYLE_PRIMARY);
  popup_cell_utils::FormatLabel(*main_text_label, kSuggestion.main_text,
                                GetController());
  popup_cell_utils::AddSuggestionContentToView(
      kSuggestion, std::move(main_text_label),
      popup_cell_utils::CreateMinorTextLabel(kSuggestion.minor_text),
      /*description_label=*/nullptr,
      popup_cell_utils::CreateAndTrackSubtextViews(*view, GetController(),
                                                   GetLineNumber()),
      *view);

  // Prepare the callbacks to the controller.
  popup_cell_utils::AddCallbacksToContentView(GetController(), GetLineNumber(),
                                              *view);

  return view;
}

std::unique_ptr<PopupCellView>
PopupSuggestionStrategy::CreateDeleteAutocompleteRow() {
  if (!GetController()) {
    return nullptr;
  }
  std::unique_ptr<PopupAutocompleteCellView> view =
      std::make_unique<PopupAutocompleteCellView>(GetController(),
                                                  GetLineNumber());
  view->SetAccessibilityDelegate(
      std::make_unique<ContentItemAccessibilityDelegate>(GetController(),
                                                         GetLineNumber()));
  return view;
}

// This method is currently not implemented but we will re-evaluate it (probably
// implement it) when granular filling starts its implementation phase.
std::unique_ptr<PopupCellView> PopupSuggestionStrategy::CreateControl() {
  return nullptr;
}

/************************ PopupPasswordSuggestionStrategy *******************/

PopupPasswordSuggestionStrategy::PopupPasswordSuggestionStrategy(
    base::WeakPtr<AutofillPopupController> controller,
    int line_number)
    : PopupRowBaseStrategy(std::move(controller), line_number) {}

PopupPasswordSuggestionStrategy::~PopupPasswordSuggestionStrategy() = default;

std::unique_ptr<PopupCellView>
PopupPasswordSuggestionStrategy::CreateContent() {
  if (!GetController()) {
    return nullptr;
  }

  const Suggestion& kSuggestion =
      GetController()->GetSuggestionAt(GetLineNumber());
  std::unique_ptr<PopupCellView> view =
      views::Builder<PopupCellView>()
          .SetAccessibilityDelegate(
              std::make_unique<ContentItemAccessibilityDelegate>(
                  GetController(), GetLineNumber()))
          .Build();

  // Add the actual views.
  std::unique_ptr<views::Label> main_text_label =
      popup_cell_utils::CreateMainTextLabel(
          kSuggestion.main_text, views::style::TextStyle::STYLE_PRIMARY);
  main_text_label->SetMaximumWidthSingleLine(kAutofillPopupUsernameMaxWidth);

  popup_cell_utils::AddSuggestionContentToView(
      kSuggestion, std::move(main_text_label),
      popup_cell_utils::CreateMinorTextLabel(kSuggestion.minor_text),
      CreateDescriptionLabel(), CreateAndTrackSubtextViews(*view), *view);

  // Prepare the callbacks to the controller.
  popup_cell_utils::AddCallbacksToContentView(GetController(), GetLineNumber(),
                                              *view);

  return view;
}

std::unique_ptr<views::Label>
PopupPasswordSuggestionStrategy::CreateDescriptionLabel() const {
  const Suggestion& kSuggestion =
      GetController()->GetSuggestionAt(GetLineNumber());
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

std::vector<std::unique_ptr<views::View>>
PopupPasswordSuggestionStrategy::CreateAndTrackSubtextViews(
    PopupCellView& content_view) const {
  std::unique_ptr<views::Label> label = std::make_unique<views::Label>(
      GetController()->GetSuggestionAt(GetLineNumber()).additional_label,
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
  label->SetElideBehavior(gfx::TRUNCATE);
  label->SetMaximumWidthSingleLine(kAutofillPopupPasswordMaxWidth);
  content_view.TrackLabel(label.get());
  std::vector<std::unique_ptr<views::View>> result;
  result.push_back(std::move(label));
  return result;
}

std::unique_ptr<PopupCellView>
PopupPasswordSuggestionStrategy::CreateControl() {
  return nullptr;
}

/************************** PopupFooterStrategy ******************************/

PopupFooterStrategy::PopupFooterStrategy(
    base::WeakPtr<AutofillPopupController> controller,
    int line_number)
    : PopupRowBaseStrategy(std::move(controller), line_number) {}

PopupFooterStrategy::~PopupFooterStrategy() = default;

std::unique_ptr<PopupCellView> PopupFooterStrategy::CreateContent() {
  if (!GetController()) {
    return nullptr;
  }

  const Suggestion& kSuggestion =
      GetController()->GetSuggestionAt(GetLineNumber());
  std::unique_ptr<PopupCellView> view =
      views::Builder<PopupCellView>()
          .SetAccessibilityDelegate(
              std::make_unique<ContentItemAccessibilityDelegate>(
                  GetController(), GetLineNumber()))
          .Build();

  views::BoxLayout* layout_manager =
      view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          popup_cell_utils::GetMarginsForContentCell(
              /*has_control_element=*/false)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  std::unique_ptr<views::ImageView> icon =
      popup_cell_utils::GetIconImageView(kSuggestion);

  const bool kUseLeadingIcon =
      base::Contains(kItemTypesUsingLeadingIcons, kSuggestion.popup_item_id);

  if (kSuggestion.is_loading) {
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
      popup_cell_utils::CreateMainTextLabel(kSuggestion.main_text,
                                            views::style::STYLE_SECONDARY);
  main_text_label->SetEnabled(!kSuggestion.is_loading);
  view->TrackLabel(view->AddChildView(std::move(main_text_label)));

  popup_cell_utils::AddSpacerWithSize(*view, *layout_manager, 0,
                                      /*resize=*/true);

  if (icon && !kUseLeadingIcon) {
    popup_cell_utils::AddSpacerWithSize(*view, *layout_manager,
                                        PopupBaseView::GetHorizontalPadding(),
                                        /*resize=*/false);
    view->AddChildView(std::move(icon));
  }

  std::unique_ptr<views::ImageView> trailing_icon =
      popup_cell_utils::GetTrailingIconImageView(kSuggestion);
  if (trailing_icon) {
    popup_cell_utils::AddSpacerWithSize(*view, *layout_manager,
                                        PopupBaseView::GetHorizontalPadding(),
                                        /*resize=*/true);
    view->AddChildView(std::move(trailing_icon));
  }

  // Force a refresh to ensure all the labels'styles are correct.
  view->RefreshStyle();

  popup_cell_utils::AddCallbacksToContentView(GetController(), GetLineNumber(),
                                              *view);

  return view;
}

std::unique_ptr<PopupCellView> PopupFooterStrategy::CreateControl() {
  return nullptr;
}

}  // namespace autofill
