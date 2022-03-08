// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_service.h"

#include "base/notreached.h"
#include "components/user_notes/user_notes_features.h"

namespace user_notes {

UserNoteService::UserNoteService() = default;

UserNoteService::~UserNoteService() = default;

void UserNoteService::OnNoteFocused(const std::string& guid) {
  DCHECK(IsUserNotesEnabled());
  NOTIMPLEMENTED();
}

void UserNoteService::OnNoteCreationDone(const std::string& guid,
                                         const std::string& note_content) {
  DCHECK(IsUserNotesEnabled());
  NOTIMPLEMENTED();
}

void UserNoteService::OnNoteCreationCancelled(const std::string& guid) {
  DCHECK(IsUserNotesEnabled());
  NOTIMPLEMENTED();
}

}  // namespace user_notes
