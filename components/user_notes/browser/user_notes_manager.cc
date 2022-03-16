// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_notes_manager.h"

#include "base/memory/ptr_util.h"
#include "components/user_notes/browser/user_note_instance.h"
#include "content/public/browser/page.h"

namespace user_notes {

// static
std::unique_ptr<UserNotesManager> UserNotesManager::CreateForTest(
    content::Page& page,
    base::SafeRef<UserNoteService> service) {
  return base::WrapUnique(new UserNotesManager(page, service));
}

UserNotesManager::UserNotesManager(content::Page& page,
                                   base::SafeRef<UserNoteService> service)
    : PageUserData<UserNotesManager>(page), service_(service) {}

UserNotesManager::~UserNotesManager() {
  for (const auto& entry_it : instance_map_) {
    DCHECK(entry_it.second->model())
        << "Encountered a note instance without proper backing model";
    service_->OnNoteInstanceRemovedFromPage(entry_it.second->model()->guid(),
                                            this);
  }
}

UserNoteInstance* UserNotesManager::GetNoteInstance(const std::string& guid) {
  const auto& entry_it = instance_map_.find(guid);
  if (entry_it == instance_map_.end()) {
    return nullptr;
  }

  return entry_it->second.get();
}

const std::vector<UserNoteInstance*> UserNotesManager::GetAllNoteInstances() {
  std::vector<UserNoteInstance*> notes;
  notes.reserve(instance_map_.size());
  for (const auto& entry_it : instance_map_) {
    notes.push_back(entry_it.second.get());
  }

  return notes;
}

void UserNotesManager::RemoveNote(const std::string& guid) {
  const auto& entry_it = instance_map_.find(guid);
  DCHECK(entry_it != instance_map_.end())
      << "Attempted to remove a note instance from a page where it didn't "
         "exist";

  service_->OnNoteInstanceRemovedFromPage(guid, this);
  instance_map_.erase(entry_it);
}

void UserNotesManager::AddNoteInstance(std::unique_ptr<UserNoteInstance> note) {
  DCHECK(note->model())
      << "Encountered a note instance without proper backing model";
  DCHECK(instance_map_.find(note->model()->guid()) == instance_map_.end())
      << "Attempted to add a note instance for the same note to the same page "
         "more than once";

  service_->OnNoteInstanceAddedToPage(note->model()->guid(), this);
  instance_map_.emplace(note->model()->guid(), std::move(note));
}

}  // namespace user_notes
