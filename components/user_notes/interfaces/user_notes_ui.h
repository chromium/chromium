// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_INTERFACES_USER_NOTES_UI_H_
#define COMPONENTS_USER_NOTES_INTERFACES_USER_NOTES_UI_H_

#include "base/supports_user_data.h"
#include "base/unguessable_token.h"
#include "ui/gfx/geometry/rect.h"

namespace user_notes {

class UserNoteInstance;

// Interface that the UI layer of User Notes must implement. Used by the
// business logic in the service to send commands to the UI.
class UserNotesUI : public base::SupportsUserData::Data {
 public:
  static const void* UserDataKey() { return &kUserDataKey; }

  UserNotesUI() = default;
  UserNotesUI(const UserNotesUI&) = delete;
  UserNotesUI& operator=(const UserNotesUI&) = delete;
  ~UserNotesUI() override = default;

  // Called when a note in the UI should be scrolled to / brought to the
  // foreground, and focused.
  virtual void FocusNote(const base::UnguessableToken& guid) = 0;

  // Called when the note creation UX should be shown in the UI layer. |bounds|
  // corresponds to the location in the webpage where the associated highlight
  // is, and should be compared with existing notes in the UI to determine where
  // the new note should be inserted.
  virtual void StartNoteCreation(UserNoteInstance* instance) = 0;

  // Called when the model has changed and the UI should consequently refresh
  // the notes it is displaying. The new model must be polled from the active
  // tab's primary page.
  virtual void InvalidateIfVisible() = 0;

  // Called by the UserNoteService when the user triggers one of the feature's
  // entry points, indicating the Notes UI should show itself.
  virtual void Show() = 0;

 private:
  static const int kUserDataKey = 0;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_INTERFACES_USER_NOTES_UI_H_
