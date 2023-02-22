// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_INTERFACES_USER_NOTES_UI_H_
#define COMPONENTS_USER_NOTES_INTERFACES_USER_NOTES_UI_H_

#include "base/supports_user_data.h"

namespace user_notes {

// Interface that the UI layer of User Notes must implement. Used by the
// business logic in the service to send commands to the UI.
class UserNotesUI : public base::SupportsUserData::Data {
 public:
  static const void* UserDataKey() { return &kUserDataKey; }

  UserNotesUI() = default;
  UserNotesUI(const UserNotesUI&) = delete;
  UserNotesUI& operator=(const UserNotesUI&) = delete;
  ~UserNotesUI() override = default;

  // Called when the tab should be changed and the note creation UX should be
  // shown in the UI layer.
  virtual void SwitchTabsAndStartNoteCreation(int tab_index) = 0;

  // Called when the note creation UX should be shown in the UI layer.
  virtual void StartNoteCreation() = 0;

  // Called by the UserNoteService when the user triggers one of the feature's
  // entry points, indicating the Notes UI should show itself.
  virtual void Show() = 0;

 private:
  static const int kUserDataKey = 0;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_INTERFACES_USER_NOTES_UI_H_
