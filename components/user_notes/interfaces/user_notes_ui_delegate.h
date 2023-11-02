// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_INTERFACES_USER_NOTES_UI_DELEGATE_H_
#define COMPONENTS_USER_NOTES_INTERFACES_USER_NOTES_UI_DELEGATE_H_

#include <string>

#include "base/unguessable_token.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace user_notes {

// Interface used by the UI layer (e.g. Side Panel on desktop) to delegate
// handling for some note-specific user actions.
class UserNotesUIDelegate {
 public:
  UserNotesUIDelegate() = default;
  UserNotesUIDelegate(const UserNotesUIDelegate&) = delete;
  UserNotesUIDelegate& operator=(const UserNotesUIDelegate&) = delete;
  virtual ~UserNotesUIDelegate() = default;

  // Called when a note in the UI is selected (i.e. via mouse press).
  virtual void OnNoteSelected(const base::UnguessableToken& id,
                              content::RenderFrameHost* rfh) = 0;

  // Called when the user deletes a note in the UI.
  virtual void OnNoteDeleted(const base::UnguessableToken& id) = 0;

  // Called when the user successfully creates a new note in the UI.
  virtual void OnNoteCreationDone(const base::UnguessableToken& id,
                                  const std::u16string& note_content) = 0;

  // Called when the user aborts the note creation process in the UI.
  virtual void OnNoteCreationCancelled(const base::UnguessableToken& id) = 0;

  // Called when the user edits an existing note in the UI.
  virtual void OnNoteEdited(const base::UnguessableToken& id,
                            const std::u16string& note_content) = 0;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_INTERFACES_USER_NOTES_UI_DELEGATE_H_
