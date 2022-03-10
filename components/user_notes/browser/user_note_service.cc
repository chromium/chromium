// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_service.h"

#include "base/notreached.h"
#include "components/user_notes/browser/user_notes_manager.h"
#include "components/user_notes/user_notes_features.h"

namespace user_notes {

UserNoteService::UserNoteService() = default;

UserNoteService::~UserNoteService() = default;

void UserNoteService::OnNoteInstanceAddedToPage(const std::string& guid,
                                                UserNotesManager* manager) {
  DCHECK(IsUserNotesEnabled());
  DCHECK(model_map_.find(guid) != model_map_.end())
      << "A note instance without backing model was added to a page";

  model_map_.at(guid).second.insert(manager);
}

void UserNoteService::OnNoteInstanceRemovedFromPage(const std::string& guid,
                                                    UserNotesManager* manager) {
  DCHECK(IsUserNotesEnabled());

  auto model = model_map_.find(guid);
  DCHECK(model != model_map_.end())
      << "A note model was destroyed before all its instances";

  auto deleteCount = (*model).second.second.erase(manager);
  DCHECK(deleteCount > 0) << "Attempted to remove a ref to a note manager that "
                             "wasn't in the model map";

  // If there are no longer any pages displaying this model, destroy it.
  if ((*model).second.second.empty()) {
    model_map_.erase(guid);
  }
}

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
