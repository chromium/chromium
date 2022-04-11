// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/user_note/user_note_view.h"

#include "ui/views/border.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"

UserNoteView::UserNoteView(user_notes::UserNoteInstance* user_note_instance,
                           UserNoteView::State state)
    : user_note_instance_(user_note_instance) {
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
      // TODO(cheickcisse): AddChildView new user note view
      break;
    case UserNoteView::State::kEditing:
      // TODO(cheickcisse): AddChildView edit existing user note view
      break;
    case UserNoteView::State::kOrphaned:
      // TODO(cheickcisse): AddChildView orphan existing user note
      break;
  }
}

UserNoteView::~UserNoteView() = default;

std::unique_ptr<views::View> UserNoteView::CreateTextareaView(
    std::string& note_text) {
  // TODO(cheickcisse): Implement Textarea view, which is part of the view when
  // a user adds a new user note. It will include a textarea
  return std::make_unique<views::View>();
}

std::unique_ptr<views::View> UserNoteView::CreateButtonsView() {
  // TODO(cheickcisse): Implement the buttons view, which is part of the view
  // when a user adds a new user note and included an add and cancel button.
  return std::make_unique<views::View>();
}

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
