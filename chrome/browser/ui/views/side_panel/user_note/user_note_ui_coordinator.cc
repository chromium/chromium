// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/user_note/user_note_ui_coordinator.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

UserNoteUICoordinator::UserNoteUICoordinator(Browser* browser)
    : BrowserUserData<UserNoteUICoordinator>(*browser), browser_(browser) {
  browser_->tab_strip_model()->AddObserver(this);
}

UserNoteUICoordinator::~UserNoteUICoordinator() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

void UserNoteUICoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kUserNote,
      l10n_util::GetStringUTF16(IDS_USER_NOTE_TITLE),
      ui::ImageModel::FromVectorIcon(kInkHighlighterIcon, ui::kColorIcon),
      base::BindRepeating(&UserNoteUICoordinator::CreateUserNotesView,
                          base::Unretained(this))));
}

void UserNoteUICoordinator::FocusNote(const std::string& guid) {
  // TODO(cheickcisse): Implement FocusNote, which will be called by
  // UserNoteService to inform, inform the user note side panel to scroll the
  // corresponding note into view in the side panel.
}

void UserNoteUICoordinator::StartNoteCreation(const std::string& guid,
                                              gfx::Rect bounds) {
  // TODO(cheickcisse): Implement StartNoteCreation, which will be called by
  // UserNoteService to add a new note in the side panel. The new note entry row
  // will be position at y relative to existing notes in the side panel.
}

void UserNoteUICoordinator::Invalidate() {
  // TODO(cheickcisse): Implement Invalidate, which will be called by
  // UserNoteService or by OnTabStripModelChanged to fetch the latest list of
  // notes to display from UserNotePageData associated with the active tab.
}

void UserNoteUICoordinator::Show() {
  // TODO(cheickcisse): Implement Show, which will be called by UserNoteService
  // to open notes in the side panel.
}

void UserNoteUICoordinator::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // TODO(cheickcisse): Implement OnTabStripModelChanged, which should call
  // Invalidate() to poll the latest list of notes to display.
}

std::unique_ptr<views::View> UserNoteUICoordinator::CreateUserNotesView() {
  // Layout structure:
  //
  // [| [NoteView]              | <--- scroll content view ] <--- scroll view
  // [| ...                     |]
  // [| ...                     |]

  auto root_view = std::make_unique<views::View>();
  root_view->SetLayoutManager(std::make_unique<views::FillLayout>());

  auto* scroll_view =
      root_view->AddChildView(std::make_unique<views::ScrollView>());
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  // Setting clip height is necessary to make ScrollView take into account its
  // contents' size. Using zeroes doesn't prevent it from scrolling and sizing
  // correctly.
  scroll_view->ClipHeightTo(0, 0);

  // TODO(cheickcisse): Populate scroll content view.
  scroll_contents_view_ =
      scroll_view->SetContents(std::make_unique<views::View>());

  constexpr int edge_margin = 16;
  constexpr int vertical_padding = 16;
  auto* layout = scroll_contents_view_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets::VH(vertical_padding, edge_margin), vertical_padding));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  return root_view;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(UserNoteUICoordinator);
