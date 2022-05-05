// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_VIEW_H_

#include "components/user_notes/browser/user_note_instance.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/view.h"

// View of a user note in the side panel in multiple states. The view in the
// default state draws a label with a date, a menu button, and a label with a
// text. The view in creating state draws a textarea, and two buttons (cancel
// and add). The view in the edit state is similar to the creating state but
// sets a text in the textarea. Lastly, the view in the orphaned state is
// similar to the default state but draws an additional label.
class UserNoteView : public views::View {
 public:
  enum class State {
    // State that will display an existing user note.
    kDefault,
    // State that will display a new user note.
    kCreating,
    // State that will display an existing user note to be edited.
    kEditing,
    // State that will display an orphan user note (note without a highlight in
    // the page).
    kOrphaned
  };

  explicit UserNoteView(
      user_notes::UserNoteInstance* user_note_instance,
      UserNoteView::State state = UserNoteView::State::kDefault);
  UserNoteView(const UserNoteView&) = delete;
  UserNoteView& operator=(const UserNoteView&) = delete;
  ~UserNoteView() override;

  const base::UnguessableToken& UserNoteId() {
    return user_note_instance_ != nullptr ? user_note_instance_->model().id()
                                          : base::UnguessableToken::Null();
  }

  const gfx::Rect& user_note_rect() const {
    return user_note_instance_->rect();
  }

 private:
  void OnCancelNewUserNote();
  void OnAddUserNote();

  void OnOpenMenu();

  raw_ptr<user_notes::UserNoteInstance> user_note_instance_;
  raw_ptr<views::Textarea> text_area_;
  raw_ptr<views::View> button_container_;
  raw_ptr<views::Label> user_note_body_;
  raw_ptr<views::View> user_note_header_;
  raw_ptr<views::View> user_note_quote_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_VIEW_H_
