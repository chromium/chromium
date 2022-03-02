// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_SERVICE_H_
#define COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

#include <string>

#include "components/user_notes/browser/user_notes_ui_delegate.h"

namespace user_notes {

// Keyed service cooridnating the different parts (Renderer, UI layer, storage
// layer) of the User Notes feature for the current user profile.
class UserNoteService : public KeyedService, public UserNotesUIDelegate {
 public:
  explicit UserNoteService();
  ~UserNoteService() override;
  UserNoteService(const UserNoteService&) = delete;
  UserNoteService& operator=(const UserNoteService&) = delete;

  // UserNotesUIDelegate implementation.
  void OnNoteFocused(std::string guid) override;
  void OnNoteCreationDone(std::string guid, std::string note_content) override;
  void OnNoteCreationCancelled(std::string guid) override;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_SERVICE_H_
