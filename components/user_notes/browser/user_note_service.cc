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

base::SafeRef<UserNoteService> UserNoteService::GetSafeRef() {
  return weak_ptr_factory_.GetSafeRef();
}

void UserNoteService::OnNoteInstanceAddedToPage(const std::string& guid,
                                                UserNotesManager* manager) {
  DCHECK(IsUserNotesEnabled());
  const auto& entry_it = model_map_.find(guid);
  DCHECK(entry_it != model_map_.end())
      << "A note instance without backing model was added to a page";

  entry_it->second.managers.insert(manager);
}

void UserNoteService::OnNoteInstanceRemovedFromPage(const std::string& guid,
                                                    UserNotesManager* manager) {
  DCHECK(IsUserNotesEnabled());

  const auto& entry_it = model_map_.find(guid);
  DCHECK(entry_it != model_map_.end())
      << "A note model was destroyed before all its instances";

  auto deleteCount = entry_it->second.managers.erase(manager);
  DCHECK(deleteCount > 0) << "Attempted to remove a ref to a note manager that "
                             "wasn't in the model map";

  // If there are no longer any pages displaying this model, destroy it.
  if (entry_it->second.managers.empty()) {
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

UserNoteService::ModelMapEntry::ModelMapEntry(std::unique_ptr<UserNote> m)
    : model(std::move(m)) {}

UserNoteService::ModelMapEntry::ModelMapEntry(ModelMapEntry&& other) = default;

UserNoteService::ModelMapEntry::~ModelMapEntry() = default;

}  // namespace user_notes
