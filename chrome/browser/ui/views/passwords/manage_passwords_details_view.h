// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_DETAILS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_DETAILS_VIEW_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "components/password_manager/core/browser/password_form.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

class RichHoverButton;

namespace views {
class Label;
class Separator;
class Textarea;
class Textfield;
class View;
}  // namespace views

// A view that displays the username, password and a note of password with
// buttons to copy and edit such information. It has both a reading mode, where
// data are only copyable, and edit mode where the note or an empty username can
// be edited. Used in the ManagePasswordsView.
class ManagePasswordsDetailsView : public views::BoxLayoutView {
  METADATA_HEADER(ManagePasswordsDetailsView, views::BoxLayoutView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTopView);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBackButton);

  // `password_form` is the password form to be displayed.
  // `allow_empty_username_edit` indicates whether the edit button for empty
  // usernames is available.
  // The view uses `username_exists_callback` to check if the currently entered
  // username in the edit mode already exists and hence should be considered an
  // invalid input. `switched_to_edit_mode_callback` is invoked when the user
  // decides to edit one of the editable fields in the UI. This is to inform the
  // embedder to do the necessary changes (e.g. show update/cancel button).
  // `on_activity_callback` is invoked upon user activity in the view e.g. user
  // is typing a note in the edit view, or copying a username.
  // `on_input_validation_callback` is invoked after validating user input to
  // inform the embedder if the current input is invalid.
  // `on_manage_password_clicked_callback` callback is invoked when the user
  // clicks on the "Manage password" button.
  ManagePasswordsDetailsView(
      password_manager::PasswordForm password_form,
      bool allow_empty_username_edit,
      base::RepeatingCallback<bool(const std::u16string&)>
          username_exists_callback,
      base::RepeatingClosure switched_to_edit_mode_callback,
      base::RepeatingClosure on_activity_callback,
      base::RepeatingCallback<void(bool)> on_input_validation_callback,
      base::RepeatingClosure on_manage_password_clicked_callback);

  ManagePasswordsDetailsView(const ManagePasswordsDetailsView&) = delete;
  ManagePasswordsDetailsView& operator=(const ManagePasswordsDetailsView&) =
      delete;

  ~ManagePasswordsDetailsView() override;

  // Creates the title for the details view. It consists of the back arrow icon
  // button if the `on_back_clicked_callback` is provided (which is invoked
  // when the button is clicked) and the `password_form` origin.
  static std::unique_ptr<views::View> CreateTitleView(
      const password_manager::PasswordForm& password_form,
      std::optional<base::RepeatingClosure> on_back_clicked_callback);

  // Switches to the reading mode by hiding all the editing UI controls, and
  // showing the display UI controls instead.
  void SwitchToReadingMode();

  // In edit mode, those method return the current value entered by the user.
  // Return nullopt if the correposnding fields are in the reading mode.
  std::optional<std::u16string> GetUserEnteredUsernameValue() const;
  std::optional<std::u16string> GetUserEnteredPasswordNoteValue() const;

 private:
  void SwitchToEditUsernameMode();
  void SwitchToEditNoteMode();
  void OnUserInputChanged();

  // Can be used to check whether a credential with the same username already
  // exists for this website.
  base::RepeatingCallback<bool(const std::u16string&)>
      username_exists_callback_;

  // The callback that is invoked when the user decide to edit one of the
  // editable field in the UI. This is to inform the embedder to do the
  // necessary changes (e.g. show update/cancel button).
  base::RepeatingClosure switched_to_edit_mode_callback_;

  // The callback that is invoked upon user activity in the view e.g. user is
  // typing a note in the edit view, or copying a username.
  base::RepeatingClosure on_activity_callback_;

  // The callback that is invoked after validating user input to inform the
  // embedder if the current input is invalid.
  base::RepeatingCallback<void(bool)> on_input_validation_callback_;

  raw_ptr<views::View> read_username_row_ = nullptr;
  raw_ptr<views::View> edit_username_row_ = nullptr;
  raw_ptr<views::Textfield> username_textfield_ = nullptr;
  raw_ptr<views::Label> username_error_label_ = nullptr;
  std::vector<base::CallbackListSubscription> text_changed_subscriptions_;

  raw_ptr<views::View> read_note_row_ = nullptr;
  raw_ptr<views::View> edit_note_row_ = nullptr;
  raw_ptr<views::Textarea> note_textarea_ = nullptr;
  raw_ptr<views::Label> note_error_label_ = nullptr;

  raw_ptr<views::Separator> separator_row_ = nullptr;
  raw_ptr<RichHoverButton> manage_password_row_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_DETAILS_VIEW_H_
