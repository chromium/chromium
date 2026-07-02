// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_language_search_view.h"

#include "base/i18n/string_search.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kMaxVisibleHeight = 250;
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

  list_view_ = scroll_view_->SetContents(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .Build());

  // Initialize the language list with an empty query.
  UpdateLanguageList(std::u16string());
}

TranslateLanguageSearchView::~TranslateLanguageSearchView() = default;

void TranslateLanguageSearchView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  UpdateLanguageList(new_contents);
}

void TranslateLanguageSearchView::ResetLanguageIndex(int language_index) {
  search_field_->SetText(model_->GetTargetLanguageNameAt(language_index));
  UpdateLanguageList(std::u16string(search_field_->GetText()));
}

void TranslateLanguageSearchView::CreateLanguageHoverButton(
    int language_index) {
  std::u16string name = model_->GetTargetLanguageNameAt(language_index);
  HoverButton* button = list_view_->AddChildView(std::make_unique<HoverButton>(
      base::BindRepeating(&TranslateLanguageSearchView::OnLanguageButtonPressed,
                          base::Unretained(this), language_index),
      name));
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
  ClearLanguageList();
  on_language_selected_.Run(language_index);
}

void TranslateLanguageSearchView::ClearLanguageList() {
  list_view_->RemoveAllChildViews();
  list_view_->InvalidateLayout();
}

BEGIN_METADATA(TranslateLanguageSearchView)
END_METADATA
