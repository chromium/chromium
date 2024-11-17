// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_service.h"

#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/typed_macros.h"
#include "components/user_notes/browser/frame_user_note_changes.h"
#include "components/user_notes/browser/user_note_manager.h"
#include "components/user_notes/browser/user_note_utils.h"
#include "components/user_notes/interfaces/user_note_metadata_snapshot.h"
#include "components/user_notes/interfaces/user_notes_ui.h"
#include "components/user_notes/user_notes_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "ui/gfx/geometry/rect.h"

namespace user_notes {

UserNoteService::UserNoteService(
    std::unique_ptr<UserNoteServiceDelegate> delegate,
    std::unique_ptr<UserNoteStorage> storage)
    : delegate_(std::move(delegate)), storage_(std::move(storage)) {
  // storage_ can be null in tests.
  if (storage_)
    storage_->AddObserver(this);
}

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
  // TODO(crbug.com/40832588): On browser startup, this method will be called
  // once for each tab that's being restored, potentially slowing down the
  // startup process and delaying browser responsiveness. This method should
  // probably be disabled during browser startup and re-enabled after all tabs
  // have been restored, so that note fetching for all restored tabs can be
  // batched into a single operation.

  DCHECK(IsUserNotesEnabled());

  // For now, Notes are only supported in the main frame.
  // TODO(crbug.com/40832588): This will need to be changed when User Notes are
  // supported in subframes and / or AMP viewers.
  if (!rfh->IsInPrimaryMainFrame()) {
    return;
  }

  // TODO(crbug.com/40832588): Should non-web URLs such as chrome:// and
  // file:/// also be ignored here?
  if (rfh->GetPage().GetMainDocument().IsErrorDocument()) {
    return;
  }

  TRACE_EVENT("browser", "UserNoteService::OnFrameNavigated", "URL",
              rfh->GetLastCommittedURL());

  DCHECK(UserNoteManager::GetForPage(rfh->GetPage()));

  std::vector<content::WeakDocumentPtr> frames = {rfh->GetWeakDocumentPtr()};
  UserNoteStorage::UrlSet urls = {rfh->GetLastCommittedURL()};
  storage_->GetNoteMetadataForUrls(
      std::move(urls),
      base::BindOnce(&UserNoteService::OnNoteMetadataFetchedForNavigation,
                     weak_ptr_factory_.GetWeakPtr(), frames));
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
  CHECK(entry_it != model_map_.end(), base::NotFatalUntil::M130)
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
    CHECK(entry_it != model_map_.end(), base::NotFatalUntil::M130)
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
                                         bool has_selected_text) {
  CHECK(IsUserNotesEnabled());
  CHECK(frame);
  CHECK(!frame->GetParentOrOuterDocument());
  UserNoteManager* manager = UserNoteManager::GetForPage(frame->GetPage());
  CHECK(manager);

  // TODO(crbug.com/40832588): `has_selected_text` is used to determine whether
  // or not to create a page-level note. This will need to be reassessed when
  // page-level UX is finalized. In addition, record/use
  // LinkGenerationReadyStatus.
  if (has_selected_text) {
    auto create_agent_callback = base::BindOnce(
        [](base::SafeRef<UserNoteService> service,
           content::WeakDocumentPtr document,
           blink::mojom::SelectorCreationResultPtr selector_creation_result,
           shared_highlighting::LinkGenerationError error,
           shared_highlighting::LinkGenerationReadyStatus) {
          if ((error != shared_highlighting::LinkGenerationError::kNone) !=
              (!selector_creation_result)) {
            mojo::ReportBadMessage("User note creation was invalid");
            return;
          }

          if (!selector_creation_result->host_receiver.is_valid()) {
            mojo::ReportBadMessage(
                "User note creation received an invalid receiver");
            return;
          }

          if (!selector_creation_result->agent_remote.is_valid()) {
            mojo::ReportBadMessage(
                "User note creation received an invalid remote");
            return;
          }

          if (selector_creation_result->serialized_selector.empty()) {
            mojo::ReportBadMessage(
                "User note creation received an empty selector for mojo "
                "binding result");
            return;
          }

          if (selector_creation_result->selected_text.empty()) {
            mojo::ReportBadMessage(
                "User note creation received an empty text for mojo binding "
                "result");
            return;
          }

          service->InitializeNewNoteForCreation(
              document, /*is_page_level=*/false,
              std::move(selector_creation_result->host_receiver),
              std::move(selector_creation_result->agent_remote),
              selector_creation_result->serialized_selector,
              selector_creation_result->selected_text);
        },
        // SafeRef is safe since the service owns the UserNoteManager which
        // owns the mojo binding so if we receive this callback both manager
        // and service must still be live.
        weak_ptr_factory_.GetSafeRef(), frame->GetWeakDocumentPtr());

    manager->note_agent_container()->CreateAgentFromSelection(
        blink::mojom::AnnotationType::kUserNote,
        std::move(create_agent_callback));
  } else {
    InitializeNewNoteForCreation(frame->GetWeakDocumentPtr(),
                                 /*is_page_level=*/true, mojo::NullReceiver(),
                                 mojo::NullRemote(),
                                 /*serialized_selector=*/"",
                                 /*selected_text=*/std::u16string());
  }
}

void UserNoteService::OnWebHighlightFocused(const base::UnguessableToken& id,
                                            content::RenderFrameHost* rfh) {
  // TODO(crbug.com/40062727): Remove this during notes backend cleanup.
}

void UserNoteService::OnNoteSelected(const base::UnguessableToken& id,
                                     content::RenderFrameHost* rfh) {
  DCHECK(IsUserNotesEnabled());
  DCHECK(rfh);
  UserNoteManager* manager = UserNoteManager::GetForPage(rfh->GetPage());
  DCHECK(manager);
  UserNoteInstance* note_instance = manager->GetNoteInstance(id);
  DCHECK(note_instance);
  note_instance->OnNoteSelected();
}

void UserNoteService::OnNoteDeleted(const base::UnguessableToken& id) {
  DCHECK(IsUserNotesEnabled());
  storage_->DeleteNote(id);
}

void UserNoteService::OnNoteCreationDone(const base::UnguessableToken& id,
                                         const std::u16string& note_content) {
  DCHECK(IsUserNotesEnabled());

  // Retrieve the partial note from the creation map and send it to the storage
  // layer so it can officially be created and persisted. This will trigger a
  // note change event, which will cause the service to propagate this new note
  // to all relevant pages via `FrameUserNoteChanges::Apply()`. The partial
  // model will be cleaned up from the creation map as part of that process.
  const auto& creation_entry_it = creation_map_.find(id);
  CHECK(creation_entry_it != creation_map_.end(), base::NotFatalUntil::M130)
      << "Attempted to complete the creation of a note that doesn't exist";
  const UserNote* note = creation_entry_it->second.model.get();
  if (!note)
    return;
  storage_->UpdateNote(note, note_content, /*is_creation=*/true);
}

void UserNoteService::OnNoteCreationCancelled(
    const base::UnguessableToken& id) {
  DCHECK(IsUserNotesEnabled());

  // Simply remove the instance from its manager. This will in turn call
  // `OnNoteInstanceRemovedFromPage`, which will clean up the partial model from
  // the creation map.
  const auto& entry_it = creation_map_.find(id);
  CHECK(entry_it != creation_map_.end(), base::NotFatalUntil::M130)
      << "Attempted to cancel the creation of a note that doesn't exist";
  DCHECK_EQ(entry_it->second.managers.size(), 1u)
      << "Unexpectedly had more than one manager ref in the creation map for a "
         "partial note.";

  (*entry_it->second.managers.begin())->RemoveNote(id);
}

void UserNoteService::OnNoteEdited(const base::UnguessableToken& id,
                                   const std::u16string& note_content) {
  DCHECK(IsUserNotesEnabled());
  const UserNote* note = GetNoteModel(id);
  if (!note)
    return;
  storage_->UpdateNote(note, note_content);
}

void UserNoteService::OnNotesChanged() {
  std::vector<content::RenderFrameHost*> all_frames =
      delegate_->GetAllFramesForUserNotes();
  UserNoteStorage::UrlSet urls;
  std::vector<content::WeakDocumentPtr> all_frames_weak;
  all_frames_weak.reserve(all_frames.size());

  for (content::RenderFrameHost* frame : all_frames) {
    urls.emplace(frame->GetLastCommittedURL());
    all_frames_weak.emplace_back(frame->GetWeakDocumentPtr());
  }

  storage_->GetNoteMetadataForUrls(
      std::move(urls),
      base::BindOnce(&UserNoteService::OnNoteMetadataFetched,
                     weak_ptr_factory_.GetWeakPtr(), all_frames_weak));
}

void UserNoteService::InitializeNewNoteForCreation(
    content::WeakDocumentPtr document,
    bool is_page_level,
    mojo::PendingReceiver<blink::mojom::AnnotationAgentHost> host_receiver,
    mojo::PendingRemote<blink::mojom::AnnotationAgent> agent_remote,
    const std::string& serialized_selector,
    const std::u16string& selected_text) {
  content::RenderFrameHost* frame = document.AsRenderFrameHostIfValid();
  if (!frame)
    return;

  UserNoteManager* manager = UserNoteManager::GetForPage(frame->GetPage());
  DCHECK(manager);

  // If attachment succeeded, the returned mojo endpoints must all be valid and
  // the selector/text must be non empty. If attachment failed (or wasn't
  // attempted since the note is a kPage type) these will all be invalid/empty.
  bool has_renderer_agent = agent_remote.is_valid();

  DCHECK_EQ(has_renderer_agent, host_receiver.is_valid());
  DCHECK_NE(has_renderer_agent, serialized_selector.empty());
  DCHECK_NE(has_renderer_agent, selected_text.empty());

  // If this is a page-level note, we must not have a renderer agent. If we
  // received a renderer agent, it must be a text-level note.
  DCHECK(!is_page_level || !has_renderer_agent);
  DCHECK(!has_renderer_agent || !is_page_level);

  // If this is a text-targeted note and we didn't receive back an agent,
  // selector generation must have failed. For now, simply abort.
  // TODO(crbug.com/40832588): Decide how to handle the case where a selector
  // for the selected text couldn't be generated. (
  if (!is_page_level && !has_renderer_agent)
    return;

  auto target = std::make_unique<UserNoteTarget>(
      is_page_level ? UserNoteTarget::TargetType::kPage
                    : UserNoteTarget::TargetType::kPageText,
      selected_text, frame->GetLastCommittedURL(), serialized_selector);

  // TODO(gujen): This partial note creation logic will be moved to an API
  // exposed by the storage layer in order to keep the creation of UserNote
  // models centralized. However, until the storage layer is finished, manually
  // create a partial note here.
  base::Time now = base::Time::Now();
  int note_version = 1;
  auto metadata = std::make_unique<UserNoteMetadata>(now, now, note_version);
  auto body = std::make_unique<UserNoteBody>(/*plain_text_value=*/u"");

  auto partial_note = std::make_unique<UserNote>(
      base::UnguessableToken::Create(), std::move(metadata), std::move(body),
      std::move(target));

  std::unique_ptr<UserNoteInstance> instance =
      UserNoteInstance::Create(partial_note->GetSafeRef(), manager);

  // When attachment completes the instance will have received its rect on the
  // page so the UI note creation flow can begin at that point. Note, this
  // callback is guaranteed to be invoked after the current method returns (so
  // after the creation map is updated and instance added to its manager).
  auto attachment_finished_callback = base::BindOnce(
      [](base::SafeRef<UserNoteService> service,
         content::WeakDocumentPtr document, UserNoteInstance& instance) {
        content::RenderFrameHost* frame = document.AsRenderFrameHostIfValid();

        // TODO(bokan): delegate_ can be nullptr in unit tests - should we mock
        // it?
        if (!service->delegate_ || !frame)
          return;

        // Finally, notify the UI layer that it should start the note creation
        // UX for this note. The UI layer will eventually call either
        // `OnNoteCreationDone` or `OnNoteCreationCancelled`, in which the
        // partial note will be finalized or deleted, respectively.
        if (service->delegate_->GetUICoordinatorForFrame(frame)) {
          // TODO(crbug.com/40062727): Remove this during notes backend cleanup.
        }
      },
      // SafeRef is safe for the service since it owns the manager which owns
      // the instance.
      weak_ptr_factory_.GetSafeRef(), frame->GetWeakDocumentPtr(),
      // std::ref is safe since it owns the mojo endpoint which will cause this
      // invocation (so if it's deleted this callback won't be invoked).
      std::ref(*instance));

  if (!is_page_level) {
    DCHECK(has_renderer_agent);
    instance->BindToHighlight(std::move(host_receiver), std::move(agent_remote),
                              std::move(attachment_finished_callback));
    // Silence use-after-move warning; if this path is taken, we want to pass a
    // null callback in AddNoteInstance.
    attachment_finished_callback = base::NullCallback();
  }

  // Store the partial note model into the creation map (not the model map)
  // until it is finalized.
  UserNoteService::ModelMapEntry entry(std::move(partial_note));
  entry.managers.emplace(manager);
  DCHECK(creation_map_.find(entry.model->id()) == creation_map_.end())
      << "Attempted to create a partial note that already exists";
  creation_map_.emplace(entry.model->id(), std::move(entry));

  manager->AddNoteInstance(std::move(instance),
                           std::move(attachment_finished_callback));
}

void UserNoteService::OnNoteMetadataFetchedForNavigation(
    const std::vector<content::WeakDocumentPtr>& all_frames,
    UserNoteMetadataSnapshot metadata_snapshot) {
  TRACE_EVENT("browser", "UserNoteService::OnNoteMetadataFetchedForNavigation");
  DCHECK(all_frames.size() == 1u);

  content::RenderFrameHost* rfh = all_frames[0].AsRenderFrameHostIfValid();

  if (!rfh) {
    // The navigated frame is no longer valid.
    return;
  }

  TRACE_EVENT_INSTANT("browser", "Valid Frame", "URL",
                      rfh->GetLastCommittedURL(), "Active",
                      delegate_->IsFrameInActiveTab(rfh), "HasNoteMetadata",
                      !metadata_snapshot.IsEmpty());

  if (delegate_->IsFrameInActiveTab(rfh)) {
    UserNotesUI* ui = delegate_->GetUICoordinatorForFrame(rfh);
    if (!ui) {
      return;
    }

    // TODO(crbug.com/40832588): For now, always invalidate the UI if the tab is
    // in the foreground. This is to fix edge cases around back/forward
    // navigations, where the Page (and attached UserNoteManager) is kept alive
    // in the BFCache. If the notes didn't change on disk by the time the user
    // does a back/forward navigation, InvalidateIfVisible() will never get
    // called because there won't be any diff between the instances in the Page
    // and the notes on disk. Ideally, InvalidateIfVisible() should only be
    // called if this is a back/forward navigation and the notes didn't change,
    // but there's no way to know whether the notes changed until further down
    // the callback stack. Since InvalidateIfVisible() is cheap enough, always
    // calling it here is considered an acceptable fix for now.
    TRACE_EVENT_INSTANT("browser", "Invalidate UI");
    // TODO(crbug.com/40062727): Remove this during notes backend cleanup.

    if (!metadata_snapshot.IsEmpty()) {
      // TODO(crbug.com/40832588): For now, automatically activate User Notes UI
      // when the user navigates to a page with notes. Before launch though,
      // this should be changed to a popup / notification that the user must
      // interact with to launch the notes UI.
      // TODO(crbug.com/40062727): Remove this during notes backend cleanup.
    }
  }

  if (!metadata_snapshot.IsEmpty()) {
    OnNoteMetadataFetched(all_frames, std::move(metadata_snapshot));
  }
}

void UserNoteService::OnNoteMetadataFetched(
    const std::vector<content::WeakDocumentPtr>& all_frames,
    UserNoteMetadataSnapshot metadata_snapshot) {
  std::vector<std::unique_ptr<FrameUserNoteChanges>> note_changes =
      CalculateNoteChanges(*this, all_frames, metadata_snapshot);

  // All added and modified notes must be fetched from storage to eventually be
  // put in the model map. For removed notes there is no need to update the
  // model map at this point; it will be done later when applying the changes.
  IdSet notes_to_fetch;
  IdSet new_notes;

  for (const std::unique_ptr<FrameUserNoteChanges>& diff : note_changes) {
    for (const base::UnguessableToken& note_id : diff->notes_added()) {
      notes_to_fetch.emplace(note_id);
      new_notes.emplace(note_id);
    }
    for (const base::UnguessableToken& note_id : diff->notes_modified()) {
      notes_to_fetch.emplace(note_id);
    }
  }

  storage_->GetNotesById(
      std::move(notes_to_fetch),
      base::BindOnce(&UserNoteService::OnNoteModelsFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(new_notes),
                     std::move(note_changes)));
}

void UserNoteService::OnNoteModelsFetched(
    const IdSet& new_notes,
    std::vector<std::unique_ptr<FrameUserNoteChanges>> note_changes,
    std::vector<std::unique_ptr<UserNote>> notes) {
  TRACE_EVENT("browser", "UserNoteService::OnNoteModelsFetched", "num_notes",
              notes.size());
  // Update the model map with the new models.
  for (std::unique_ptr<UserNote>& note : notes) {
    base::UnguessableToken id = note->id();
    const auto& new_note_it = new_notes.find(id);
    const auto& creation_entry_it = creation_map_.find(id);
    const auto& model_entry_it = model_map_.find(id);

    if (creation_entry_it != creation_map_.end()) {
      // This note was authored locally. It could also be in the list of new
      // notes if the URL it's attached to was loaded in multiple tabs, but it
      // cannot exist in the model map yet. Move it there from the creation map.
      DCHECK(model_entry_it == model_map_.end());
      creation_entry_it->second.model->Update(std::move(note));
      model_map_.emplace(id, std::move(creation_entry_it->second));
      creation_map_.erase(creation_entry_it);
    } else if (new_note_it == new_notes.end() ||
               model_entry_it != model_map_.end()) {
      // Either this note was updated or the URL it is attached to was already
      // loaded in another tab. Either way, its model already exists in the
      // model map, so simply update it with the latest model.
      DCHECK(creation_entry_it == creation_map_.end());
      CHECK(model_entry_it != model_map_.end(), base::NotFatalUntil::M130);
      model_entry_it->second.model->Update(std::move(note));
    } else {
      // This is a new note that wasn't authored locally. Simply add the model
      // to the model map.
      CHECK(new_note_it != new_notes.end(), base::NotFatalUntil::M130);
      DCHECK(model_entry_it == model_map_.end());
      UserNoteService::ModelMapEntry entry(std::move(note));
      model_map_.emplace(id, std::move(entry));
    }
  }

  // Now that the creation and model maps have been updated, apply all the diffs
  // to propagate the changes to the webpages and UI.
  for (std::unique_ptr<FrameUserNoteChanges>& diff : note_changes) {
    FrameUserNoteChanges* diff_raw = diff.get();
    note_changes_in_progress_.emplace(diff->id(), std::move(diff));
    diff_raw->Apply(base::BindOnce(&UserNoteService::OnFrameChangesApplied,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   diff_raw->id()));
  }
}

void UserNoteService::OnFrameChangesApplied(base::UnguessableToken change_id) {
  TRACE_EVENT("browser", "UserNoteService::OnFrameChangesApplied");
  const auto& changes_it = note_changes_in_progress_.find(change_id);
  CHECK(changes_it != note_changes_in_progress_.end(),
        base::NotFatalUntil::M130);

  const std::unique_ptr<FrameUserNoteChanges>& frame_changes =
      changes_it->second;
  const content::RenderFrameHost* rfh = frame_changes->render_frame_host();

  if (rfh && delegate_->IsFrameInActiveTab(rfh)) {
    // If this set of changes was for a page that's in an active tab, notify
    // the UI to reload the notes it's displaying.
    UserNotesUI* ui = delegate_->GetUICoordinatorForFrame(rfh);
    DCHECK(ui);
    TRACE_EVENT_INSTANT("browser", "Invalidate UI");
    // TODO(crbug.com/40062727): Remove this during notes backend cleanup.
  } else if (!rfh) {
    // The frame for these changes was deleted or navigated away; the frame was
    // removed before new note instances were added. Normally the model will be
    // removed when the last instance is removed but in this case it has no
    // instances referring back to it so it needs to be removed here.
    // TODO(bokan): We need to add browser tests and test variations of RFH
    // going away at each of the async breaks. https://crbug.com/1363310.
    for (const base::UnguessableToken& note_id : frame_changes->notes_added()) {
      const auto& entry_it = model_map_.find(note_id);
      if (entry_it == model_map_.end())
        continue;

      if (entry_it->second.managers.empty()) {
        model_map_.erase(note_id);
      }
    }
  }

  note_changes_in_progress_.erase(changes_it);
}

UserNoteService::ModelMapEntry::ModelMapEntry(std::unique_ptr<UserNote> model)
    : model(std::move(model)) {}

UserNoteService::ModelMapEntry::ModelMapEntry(ModelMapEntry&& other) = default;

UserNoteService::ModelMapEntry::~ModelMapEntry() = default;

}  // namespace user_notes
