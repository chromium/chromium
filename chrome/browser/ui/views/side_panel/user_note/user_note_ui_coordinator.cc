// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/user_note/user_note_ui_coordinator.h"

UserNoteUICoordinator::UserNoteUICoordinator(Browser* browser)
    : browser_(browser) {
  browser_->tab_strip_model()->AddObserver(this);
}

UserNoteUICoordinator::~UserNoteUICoordinator() {
  browser_->tab_strip_model()->RemoveObserver(this);
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
  // TODO(cheickcisse): Implement scroll view with table view layout.
  return std::make_unique<views::View>();
}
