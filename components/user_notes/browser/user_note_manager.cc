// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_manager.h"

#include "base/memory/ptr_util.h"
#include "components/user_notes/browser/user_note_instance.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace user_notes {

UserNoteManager::UserNoteManager(content::Page& page,
                                 base::SafeRef<UserNoteService> service)
    : PageUserData<UserNoteManager>(page), service_(service) {
  // TODO(crbug.com/1313967): If / when user notes are supported in subframes,
  // caching the agent container of the primary main frame will not work. In
  // that case, the frame's container will probably need to be fetched on each
  // note instance initialization in
  // UserNoteInstance::InitializeHighlightInternal.
  content::RenderFrameHost& frame = page.GetMainDocument();
  frame.GetRemoteInterfaces()->GetInterface(
      note_agent_container_.BindNewPipeAndPassReceiver());
}

UserNoteManager::~UserNoteManager() {
  for (const auto& entry_it : instance_map_) {
    service_->OnNoteInstanceRemovedFromPage(entry_it.second->model().id(),
                                            this);
  }
}

UserNoteInstance* UserNoteManager::GetNoteInstance(
    const base::UnguessableToken& id) {
  const auto& entry_it = instance_map_.find(id);
  if (entry_it == instance_map_.end()) {
    return nullptr;
  }

  return entry_it->second.get();
}

const std::vector<UserNoteInstance*> UserNoteManager::GetAllNoteInstances() {
  std::vector<UserNoteInstance*> notes;
  notes.reserve(instance_map_.size());
  for (const auto& entry_it : instance_map_) {
    notes.push_back(entry_it.second.get());
  }

  return notes;
}

void UserNoteManager::RemoveNote(const base::UnguessableToken& id) {
  const auto& entry_it = instance_map_.find(id);
  DCHECK(entry_it != instance_map_.end())
      << "Attempted to remove a note instance from a page where it didn't "
         "exist";

  service_->OnNoteInstanceRemovedFromPage(id, this);
  instance_map_.erase(entry_it);
}

void UserNoteManager::AddNoteInstance(std::unique_ptr<UserNoteInstance> note) {
  AddNoteInstance(std::move(note), base::DoNothing());
}

void UserNoteManager::AddNoteInstance(
    std::unique_ptr<UserNoteInstance> note_instance,
    base::OnceClosure initialize_callback) {
  // TODO(crbug.com/1313967): This DCHECK is only applicable if notes are only
  // supported in the top-level frame. If notes are ever supported in subframes,
  // it is possible for the same note ID to be added to the same page more than
  // once. For example, if website A has notes and website B embeds website A
  // multiple times via iframes, then notes in website A will be added to this
  // manager once for each frame.
  DCHECK(instance_map_.find(note_instance->model().id()) == instance_map_.end())
      << "Attempted to add a note instance for the same note to the same page "
         "more than once";

  service_->OnNoteInstanceAddedToPage(note_instance->model().id(), this);
  UserNoteInstance* note_instance_raw = note_instance.get();
  instance_map_.emplace(note_instance->model().id(), std::move(note_instance));
  note_instance_raw->InitializeHighlightIfNeeded(
      std::move(initialize_callback));
}

PAGE_USER_DATA_KEY_IMPL(UserNoteManager);

}  // namespace user_notes
