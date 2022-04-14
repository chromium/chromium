// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/user_note/user_note_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"

namespace {

// Layout structure:
//
// [    cancel add]  <--- button container
std::unique_ptr<views::View> CreateButtonsView(
    base::RepeatingClosure cancel_callback,
    base::RepeatingClosure add_callback) {
  auto button_container = std::make_unique<views::View>();

  // Cancel button
  gfx::Insets button_padding = gfx::Insets::VH(6, 16);
  auto cancel_button = std::make_unique<views::MdTextButton>(
      cancel_callback, l10n_util::GetStringUTF16(IDS_CANCEL));
  cancel_button->SetCustomPadding(button_padding);
  cancel_button->SetEnabledTextColors(SK_ColorBLUE);
  button_container->AddChildView(std::move(cancel_button));

  // Add button
  auto add_button = std::make_unique<views::MdTextButton>(
      add_callback, l10n_util::GetStringUTF16(IDS_ADD));
  add_button->SetCustomPadding(button_padding);
  add_button->SetProminent(true);
  button_container->AddChildView(std::move(add_button));

  // Button container layout
  ChromeLayoutProvider* const chrome_layout_provider =
      ChromeLayoutProvider::Get();
  auto& button_layout =
      button_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
          ->SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
          .SetDefault(
              views::kMarginsKey,
              gfx::Insets::TLBR(
                  chrome_layout_provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
                  chrome_layout_provider->GetDistanceMetric(
                      views::DISTANCE_RELATED_BUTTON_HORIZONTAL),
                  0, 0))
          .SetIgnoreDefaultMainAxisMargins(true);
  button_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(button_layout.GetDefaultFlexRule()));

  return button_container;
}

std::unique_ptr<views::Textarea> CreateTextarea() {
  auto text_area = std::make_unique<views::Textarea>();

  // Textarea container view
  int default_textarea_width = 60;
  int minimum_textarea_width = 30;
  text_area->SetDefaultWidthInChars(default_textarea_width);
  text_area->SetMinimumWidthInChars(minimum_textarea_width);
  text_area->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_ADD_NEW_USER_NOTE_PLACEHOLDER_TEXT));
  text_area->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
  text_area->SetBackgroundColor(SK_ColorTRANSPARENT);
  text_area->SetBorder(views::NullBorder());
  // Textarea container layout
  text_area->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kVertical,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred, true,
                               views::MinimumFlexSizeRule::kScaleToMinimum));

  return text_area;
}

}  // namespace

UserNoteView::UserNoteView(user_notes::UserNoteInstance* user_note_instance,
                           UserNoteView::State state)
    : user_note_instance_(user_note_instance) {
  DCHECK(state != UserNoteView::State::kEditing);
  const gfx::Insets new_user_note_contents_insets = gfx::Insets::VH(16, 20);
  const int corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  const int rounded_border_thickness = 1;
  const SkColor border_color = UserNoteView::State::kCreating == state ||
                                       UserNoteView::State::kEditing == state
                                   ? SK_ColorBLUE
                                   : SK_ColorLTGRAY;

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(new_user_note_contents_insets);
  SetBorder(views::CreateRoundedRectBorder(rounded_border_thickness,
                                           corner_radius, border_color));

  switch (state) {
    case UserNoteView::State::kDefault:
      // TODO(cheickcisse): AddChildView existing user note view
      break;
    case UserNoteView::State::kCreating:
      text_area_ = AddChildView(CreateTextarea());
      button_container_ = AddChildView(CreateButtonsView(
          base::BindRepeating(&UserNoteView::OnCancelNewUserNote,
                              base::Unretained(this)),
          base::BindRepeating(&UserNoteView::OnAddUserNote,
                              base::Unretained(this))));
      break;
    case UserNoteView::State::kOrphaned:
      // TODO(cheickcisse): AddChildView orphan existing user note
      break;
    default:
      SetBorder(nullptr);
      SetLayoutManager(nullptr);
      break;
  }
}

UserNoteView::~UserNoteView() = default;

std::unique_ptr<views::View> UserNoteView::CreateQuoteView(
    std::string& note_quote) {
  // TODO(cheickcisse): Implement the quote view, which is part of orphan
  // existing user note view It will include a label.
  return std::make_unique<views::View>();
}

std::unique_ptr<views::View> UserNoteView::CreateHeaderView(
    std::string& note_date) {
  // TODO(cheickcisse): Implement the date view, which is part of both the
  // orphan and non orphan existing user note view. It will include a label and
  // a button.
  return std::make_unique<views::View>();
}

std::unique_ptr<views::View> UserNoteView::CreateBodyView(
    std::string& note_text) {
  // TODO(cheickcisse): Implement the body view, which is part of both the
  // orphan and non orphan existing user note view. It will include a label.
  return std::make_unique<views::View>();
}

void UserNoteView::OnCancelNewUserNote() {
  // TODO(cheickcisse): Cancel adding a new note.
}

void UserNoteView::OnAddUserNote() {
  // TODO(cheickcisse): Add a new note.
}
