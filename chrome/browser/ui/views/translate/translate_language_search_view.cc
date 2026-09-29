// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_language_search_view.h"

#include <algorithm>

#include "base/i18n/string_search.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {
constexpr int kMaxVisibleHeight = 250;

bool HasModifierKeys(const ui::KeyEvent& event) {
  return event.IsShiftDown() || event.IsControlDown() || event.IsAltDown() ||
         event.IsAltGrDown() || event.IsCommandDown();
}

void HandleDownArrowKey(const std::vector<views::View*>& buttons,
                        size_t index) {
  if (index + 1 < buttons.size()) {
    buttons[index + 1]->RequestFocus();
  }
}

void HandleUpArrowKey(const std::vector<views::View*>& buttons,
                      size_t index,
                      views::View* search_field) {
  if (index > 0) {
    buttons[index - 1]->RequestFocus();
  } else {
    search_field->RequestFocus();
  }
}
}  // namespace

TranslateLanguageSearchView::TranslateLanguageSearchView(
    TranslateBubbleModel* model,
    const std::vector<std::string>& recent_target_codes,
    base::RepeatingCallback<void(int)> on_language_selected)
    : model_(model),
      recent_target_codes_(recent_target_codes),
      on_language_selected_(std::move(on_language_selected)) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  views::BoxLayoutView* search_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  search_container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  search_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  search_container->SetBorder(
      views::CreateRoundedRectBorder(1, 4, ui::kColorFocusableBorderUnfocused));

  auto magnifier =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kSearchOldIcon, ui::kColorIcon, 16));
  magnifier->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 8)));
  search_container->AddChildView(std::move(magnifier));

  search_field_ =
      search_container->AddChildView(std::make_unique<views::Textfield>());
  search_field_->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_SEARCH_LANGUAGES));
  search_field_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_SEARCH_LANGUAGES));
  search_field_->set_controller(this);
  search_field_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(8, 8)));
  search_container->SetFlexForView(search_field_, 1);

  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->ClipHeightTo(0, kMaxVisibleHeight);
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view_->SetAllowKeyboardScrolling(false);

  list_view_ = scroll_view_->SetContents(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .Build());

  // Initialize the language list with an empty query.
  UpdateLanguageList(std::u16string());
}

TranslateLanguageSearchView::~TranslateLanguageSearchView() = default;

// Focus on the search field when TranslateLanguageSearchView is focused.
void TranslateLanguageSearchView::RequestFocus() {
  if (search_field_) {
    search_field_->RequestFocus();
  }
}

bool TranslateLanguageSearchView::OnKeyPressed(const ui::KeyEvent& event) {
  if (HasModifierKeys(event) ||
      (event.key_code() != ui::VKEY_DOWN && event.key_code() != ui::VKEY_UP)) {
    return false;
  }

  std::vector<views::View*> buttons = GetLanguageButtons();
  auto it = std::ranges::find(buttons, GetFocusManager()->GetFocusedView());
  if (it == buttons.end()) {
    return false;
  }

  size_t index = static_cast<size_t>(std::distance(buttons.begin(), it));
  if (event.key_code() == ui::VKEY_DOWN) {
    HandleDownArrowKey(buttons, index);
  } else {
    HandleUpArrowKey(buttons, index, search_field_);
  }
  return true;
}

void TranslateLanguageSearchView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  UpdateLanguageList(new_contents);
}

bool TranslateLanguageSearchView::HandleKeyEvent(
    views::Textfield* /*sender*/,
    const ui::KeyEvent& key_event) {
  // Intercept VKEY_DOWN events on the search_field to move the focus
  // to the first language (before views::Textfield consumes the event).
  // Do not intercept VKEY_UP events on the search_field (keep the focus on it).
  if (key_event.type() != ui::EventType::kKeyPressed ||
      HasModifierKeys(key_event) || key_event.key_code() != ui::VKEY_DOWN) {
    return false;
  }

  std::vector<views::View*> buttons = GetLanguageButtons();
  if (buttons.empty()) {
    return false;
  }

  buttons.front()->RequestFocus();
  return true;
}

void TranslateLanguageSearchView::ResetLanguageIndex(int language_index) {
  search_field_->SetText(model_->GetTargetLanguageNameAt(language_index));
  UpdateLanguageList(std::u16string(search_field_->GetText()));
  // Whenever the "Reset" button is disabled, the Focus Manager tries to
  // move the focus to the Done button. However, the focus should be placed in
  // the search_field.
  RequestFocus();
}

void TranslateLanguageSearchView::CreateLanguageHoverButton(
    int language_index) {
  std::u16string name = model_->GetTargetLanguageNameAt(language_index);
  HoverButton* button = list_view_->AddChildView(std::make_unique<HoverButton>(
      base::BindRepeating(&TranslateLanguageSearchView::OnLanguageButtonPressed,
                          base::Unretained(this), language_index),
      name));
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  button->SetProperty(views::kMarginsKey, gfx::Insets::VH(4, 8));
}

void TranslateLanguageSearchView::UpdateLanguageList(
    const std::u16string& query) {
  list_view_->RemoveAllChildViews();
  // Render recent target languages at the top if the search box is empty.
  if (!recent_target_codes_.empty() && query.empty()) {
    for (const std::string& code : recent_target_codes_) {
      std::optional<size_t> index = model_->GetTargetLanguageIndexForCode(code);
      if (!index.has_value()) {
        continue;
      }
      CreateLanguageHoverButton(index.value());
    }
    // Add a separator between recent and all target languages.
    list_view_->AddChildView(std::make_unique<views::Separator>());
  }
  // Render filtered target languages by the query.
  for (int i = 0; i < model_->GetNumberOfTargetLanguages(); ++i) {
    std::u16string name = model_->GetTargetLanguageNameAt(i);
    if (!query.empty() && !base::i18n::StringSearchIgnoringCaseAndAccents(
                              query, name, nullptr, nullptr)) {
      continue;
    }
    CreateLanguageHoverButton(i);
  }

  if (list_view_->children().empty()) {
    views::Label* no_results_label =
        list_view_->AddChildView(std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_NO_RESULTS),
            views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
    no_results_label->SetProperty(views::kMarginsKey, gfx::Insets::VH(16, 8));
  }

  list_view_->InvalidateLayout();
}

void TranslateLanguageSearchView::OnLanguageButtonPressed(int language_index) {
  search_field_->SetText(model_->GetTargetLanguageNameAt(language_index));
  // When a language button is selected, ClearLanguageList() removes the
  // focused button from the view hierarchy, causing the Focus Manager to clear
  // focus (nullptr). Place focus back in search_field_ so keyboard focus is
  // not lost.
  RequestFocus();
  ClearLanguageList();
  on_language_selected_.Run(language_index);
}

void TranslateLanguageSearchView::ClearLanguageList() {
  list_view_->RemoveAllChildViews();
  list_view_->InvalidateLayout();
}

std::vector<views::View*> TranslateLanguageSearchView::GetLanguageButtons()
    const {
  std::vector<views::View*> buttons;
  for (views::View* child : list_view_->children()) {
    // Only return language buttons so that non-interactive items (like
    // labels and separators) are skipped during keyboard navigation.
    if (views::AsViewClass<HoverButton>(child)) {
      buttons.push_back(child);
    }
  }
  return buttons;
}

BEGIN_METADATA(TranslateLanguageSearchView)
END_METADATA
