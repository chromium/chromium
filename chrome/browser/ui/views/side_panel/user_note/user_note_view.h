// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_VIEW_H_

#include <memory>

#include "chrome/browser/ui/views/side_panel/user_note/user_note_ui_coordinator.h"
#include "components/user_notes/browser/user_note_instance.h"

namespace views {
class Label;
class Textarea;
class View;
}  // namespace views

namespace views {
class Label;
class Textarea;
class View;
class MenuRunner;
}

namespace ui {
class MenuModel;
}

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
    // State that will display a detached user note (note without a highlight
    // on the page).
    kDetached
  };

  explicit UserNoteView(
      UserNoteUICoordinator* coordinator,
      user_notes::UserNoteInstance* user_note_instance,
      UserNoteView::State state = UserNoteView::State::kDefault);
  UserNoteView(const UserNoteView&) = delete;
  UserNoteView& operator=(const UserNoteView&) = delete;
  ~UserNoteView() override;

  const base::UnguessableToken& user_note_id() { return id_; }

  const gfx::Rect& user_note_rect() const { return rect_; }

  // views::View:
  // TODO(crbug.com/1313967): Keyboard navigation and touchscreens are currently
  // not handled.
  bool OnMousePressed(const ui::MouseEvent& event) override;

 private:
  void CreateOrUpdateNoteView(UserNoteView::State state,
                              base::Time date,
                              const std::u16string content,
                              const std::u16string quote);
  void OnCancelUserNote(UserNoteView::State state);
  void OnAddUserNote();
  void OnSaveUserNote();
  void OnOpenMenu();
  void OnMenuClosed();
  void OnEditUserNote(int event_flags);
  void OnDeleteUserNote(int event_flags);
  void OnLearnUserNote(int event_flags);
  void SetCreatingOrEditState(const std::u16string content,
                              UserNoteView::State state);
  void SetDefaultOrDetachedState(base::Time date,
                                 const std::u16string content,
                                 const std::u16string quote);

  raw_ptr<user_notes::UserNoteInstance> user_note_instance_;
  raw_ptr<views::Textarea> text_area_ = nullptr;
  raw_ptr<views::View> button_container_ = nullptr;
  raw_ptr<views::Label> user_note_body_ = nullptr;
  raw_ptr<views::View> user_note_header_ = nullptr;
  raw_ptr<views::View> user_note_quote_ = nullptr;
  raw_ptr<UserNoteUICoordinator> coordinator_ = nullptr;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::unique_ptr<ui::MenuModel> dialog_model_;
  base::UnguessableToken id_;
  gfx::Rect rect_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_VIEW_H_
