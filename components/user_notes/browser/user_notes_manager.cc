// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_notes_manager.h"

#include "base/notreached.h"
#include "components/user_notes/browser/user_note_instance.h"

namespace user_notes {

UserNotesManager::UserNotesManager(content::Page& page)
    : PageUserData<UserNotesManager>(page) {}

UserNotesManager::~UserNotesManager() = default;

UserNoteInstance* UserNotesManager::GetNoteInstance(const std::string& guid) {
  NOTIMPLEMENTED();
  return nullptr;
}

const std::vector<UserNoteInstance*> UserNotesManager::GetAllNoteInstances() {
  NOTIMPLEMENTED();
  return {};
}

void UserNotesManager::RemoveNote(const std::string& guid) {
  NOTIMPLEMENTED();
}

void UserNotesManager::AddNoteInstance(std::unique_ptr<UserNoteInstance> note) {
  NOTIMPLEMENTED();
}

}  // namespace user_notes
