// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_service.h"

#include "base/notreached.h"
#include "components/user_notes/browser/user_note_manager.h"
#include "components/user_notes/interfaces/user_notes_ui.h"
#include "components/user_notes/user_notes_features.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/gfx/geometry/rect.h"

namespace user_notes {

UserNoteService::UserNoteService(
    std::unique_ptr<UserNoteServiceDelegate> delegate)
    : delegate_(std::move(delegate)) {}

UserNoteService::~UserNoteService() = default;

base::SafeRef<UserNoteService> UserNoteService::GetSafeRef() const {
  return weak_ptr_factory_.GetSafeRef();
}

const UserNote* UserNoteService::GetNoteModel(
    const base::UnguessableToken& id) const {
  const auto& entry_it = model_map_.find(id);
  return entry_it == model_map_.end() ? nullptr : entry_it->second.model.get();
}

bool UserNoteService::IsNoteInProgress(const base::UnguessableToken& id) const {
  return creation_map_.find(id) != creation_map_.end();
}

void UserNoteService::OnFrameNavigated(content::RenderFrameHost* rfh) {
  DCHECK(IsUserNotesEnabled());

  // For now, Notes are only supported in the main frame.
  // TODO(crbug.com/1313967): This will need to be changed when User Notes are
  // supported in subframes and / or AMP viewers.
  if (!rfh->IsInPrimaryMainFrame()) {
    return;
  }

  // TODO(crbug.com/1313967): Should non-web URLs such as chrome:// and
  // file:/// also be ignored here?
  if (rfh->GetPage().GetMainDocument().IsErrorDocument()) {
    return;
  }

  DCHECK(UserNoteManager::GetForPage(rfh->GetPage()));
  NOTIMPLEMENTED();
}

void UserNoteService::OnNoteInstanceAddedToPage(
    const base::UnguessableToken& id,
    UserNoteManager* manager) {
  DCHECK(IsUserNotesEnabled());

  // If this note is in the creation map, it means it is still in progress, so
  // it won't be in the model map yet.
  const auto& creation_entry_it = creation_map_.find(id);
  if (creation_entry_it != creation_map_.end()) {
    DCHECK(model_map_.find(id) == model_map_.end());
    return;
  }

  const auto& entry_it = model_map_.find(id);
  DCHECK(entry_it != model_map_.end())
      << "A note instance without backing model was added to a page";

  entry_it->second.managers.insert(manager);
}

void UserNoteService::OnNoteInstanceRemovedFromPage(
    const base::UnguessableToken& id,
    UserNoteManager* manager) {
  DCHECK(IsUserNotesEnabled());

  // If this note was in progress, its model will be in the creation map, not
  // the model map. Look for it there first.
  const auto& creation_entry_it = creation_map_.find(id);
  if (creation_entry_it != creation_map_.end()) {
    DCHECK(model_map_.find(id) == model_map_.end());

    // Erase the whole entry, as this note has been cancelled and should no
    // longer exist.
    creation_map_.erase(creation_entry_it);
  } else {
    const auto& entry_it = model_map_.find(id);
    DCHECK(entry_it != model_map_.end())
        << "A note model was destroyed before all its instances";

    auto deleteCount = entry_it->second.managers.erase(manager);
    DCHECK_GT(deleteCount, 0u) << "Attempted to remove a ref to a note manager "
                                  "that wasn't in the model map";

    // If there are no longer any pages displaying this model, destroy it.
    if (entry_it->second.managers.empty()) {
      model_map_.erase(id);
    }
  }
}

void UserNoteService::OnAddNoteRequested(content::RenderFrameHost* frame,
                                         std::string original_text,
                                         std::string selector,
                                         gfx::Rect rect) {
  DCHECK(IsUserNotesEnabled());
  DCHECK(frame);
  UserNoteManager* manager = UserNoteManager::GetForPage(frame->GetPage());
  DCHECK(manager);

  // TODO(gujen): This partial note creation logic will be moved to an API
  // exposed by the storage layer in order to keep the creation of UserNote
  // models centralized. However, until the storage layer is finished, manually
  // create a partial note here.
  base::Time now = base::Time::Now();
  int note_version = 1;
  auto metadata = std::make_unique<UserNoteMetadata>(now, now, note_version);
  auto body = std::make_unique<UserNoteBody>(/*plain_text_value=*/"");
  auto target = std::make_unique<UserNoteTarget>(
      UserNoteTarget::TargetType::kPageText, original_text,
      GURL(frame->GetLastCommittedURL()), selector);
  auto partial_note = std::make_unique<UserNote>(
      base::UnguessableToken::Create(), std::move(metadata), std::move(body),
      std::move(target));
  UserNote* partial_note_raw = partial_note.get();

  // Store the partial note model into the creation map (not the model map)
  // until it is finalized.
  UserNoteService::ModelMapEntry entry(std::move(partial_note));
  entry.managers.emplace(manager);
  DCHECK(creation_map_.find(entry.model->id()) == creation_map_.end())
      << "Attempted to create a partial note that already exists";
  creation_map_.emplace(entry.model->id(), std::move(entry));

  // Create an instance for this note so the highlight can be shown in the page,
  // and add it to the page's note manager. The instance's initialization does
  // not need to be awaited, since the highlight's rect is already known.
  auto instance = std::make_unique<UserNoteInstance>(
      partial_note_raw->GetSafeRef(), manager, rect);
  UserNoteInstance* instance_raw = instance.get();
  manager->AddNoteInstance(std::move(instance), base::DoNothing());

  // Finally, notify the UI layer that it should start the note creation UX for
  // this note. The UI layer will eventually call either `OnNoteCreationDone` or
  // `OnNoteCreationCancelled`, in which the partial note will be finalized or
  // deleted, respectively.
  UserNotesUI* ui = delegate_->GetUICoordinatorForFrame(frame);
  ui->StartNoteCreation(instance_raw);
}

void UserNoteService::OnNoteFocused(const base::UnguessableToken& id) {
  DCHECK(IsUserNotesEnabled());
  NOTIMPLEMENTED();
}

void UserNoteService::OnNoteDeleted(const base::UnguessableToken& id) {
  DCHECK(IsUserNotesEnabled());
  NOTIMPLEMENTED();
}

void UserNoteService::OnNoteCreationDone(const base::UnguessableToken& id,
                                         const std::string& note_content) {
  DCHECK(IsUserNotesEnabled());

  // Retrieve the partial note from the creation map and send it to the storage
  // layer so it can officially be created and persisted. This will trigger a
  // note change event, which will cause the service to propagate this new note
  // to all relevant pages via `FrameUserNoteChanges::Apply()`. The partial
  // model will be cleaned up from the creation map as part of that process.
  const auto& creation_entry_it = creation_map_.find(id);
  DCHECK(creation_entry_it != creation_map_.end())
      << "Attempted to complete the creation of a note that doesn't exist";
  // TODO(gujen): Call
  // UserNoteStorage::UpdateNote(entry.model, content, /*is_creation=*/true).

  // TODO(gujen): Make sure to transfer the model from the creation map to the
  // model map in the OnNotesChanged() event sent by the storage layer.
}

void UserNoteService::OnNoteCreationCancelled(
    const base::UnguessableToken& id) {
  DCHECK(IsUserNotesEnabled());

  // Simply remove the instance from its manager. This will in turn call
  // `OnNoteInstanceRemovedFromPage`, which will clean up the partial model from
  // the creation map.
  const auto& entry_it = creation_map_.find(id);
  DCHECK(entry_it != creation_map_.end())
      << "Attempted to cancel the creation of a note that doesn't exist";
  DCHECK_EQ(entry_it->second.managers.size(), 1u)
      << "Unexpectedly had more than one manager ref in the creation map for a "
         "partial note.";

  (*entry_it->second.managers.begin())->RemoveNote(id);
}

void UserNoteService::OnNoteUpdated(const base::UnguessableToken& id,
                                    const std::string& note_content) {
  DCHECK(IsUserNotesEnabled());
  NOTIMPLEMENTED();
}

UserNoteService::ModelMapEntry::ModelMapEntry(std::unique_ptr<UserNote> model)
    : model(std::move(model)) {}

UserNoteService::ModelMapEntry::ModelMapEntry(ModelMapEntry&& other) = default;

UserNoteService::ModelMapEntry::~ModelMapEntry() = default;

}  // namespace user_notes
