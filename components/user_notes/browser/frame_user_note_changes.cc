// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/frame_user_note_changes.h"

#include "base/barrier_callback.h"
#include "components/user_notes/browser/user_note_manager.h"
#include "components/user_notes/model/user_note.h"
#include "components/user_notes/model/user_note_target.h"
#include "content/public/browser/render_frame_host.h"

namespace user_notes {

FrameUserNoteChanges::FrameUserNoteChanges(
    base::SafeRef<UserNoteService> service,
    content::WeakDocumentPtr document,
    const ChangeList& notes_added,
    const ChangeList& notes_modified,
    const ChangeList& notes_removed)
    : id_(base::UnguessableToken::Create()),
      service_(service),
      document_(document),
      notes_added_(notes_added),
      notes_modified_(notes_modified),
      notes_removed_(notes_removed) {
  DCHECK(!notes_added_.empty() || !notes_modified_.empty() ||
         !notes_removed_.empty());
  DCHECK(document_.AsRenderFrameHostIfValid());
}

FrameUserNoteChanges::FrameUserNoteChanges(
    base::SafeRef<UserNoteService> service,
    content::WeakDocumentPtr document,
    ChangeList&& notes_added,
    ChangeList&& notes_modified,
    ChangeList&& notes_removed)
    : id_(base::UnguessableToken::Create()),
      service_(service),
      document_(document),
      notes_added_(std::move(notes_added)),
      notes_modified_(std::move(notes_modified)),
      notes_removed_(std::move(notes_removed)) {
  DCHECK(!notes_added_.empty() || !notes_modified_.empty() ||
         !notes_removed_.empty());
  DCHECK(document_.AsRenderFrameHostIfValid());
}

FrameUserNoteChanges::FrameUserNoteChanges(FrameUserNoteChanges&& other) =
    default;

FrameUserNoteChanges::~FrameUserNoteChanges() = default;

void FrameUserNoteChanges::Apply(base::OnceClosure callback) {
  content::RenderFrameHost* rfh = document_.AsRenderFrameHostIfValid();
  if (!rfh) {
    std::move(callback).Run();
    return;
  }

  UserNoteManager* manager = UserNoteManager::GetForPage(rfh->GetPage());
  DCHECK(manager);

  // Removed notes can be synchronously deleted from the note manager. There is
  // no need to wait for the async removal of the page highlights on the
  // renderer side.
  for (const base::UnguessableToken& note_id : notes_removed_) {
    manager->RemoveNote(note_id);
  }

  if (notes_added_.empty()) {
    std::move(callback).Run();
    return;
  }

  // For added notes, the async highlight creation on the renderer side must be
  // awaited, because the order in which notes are shown in the Notes UI depends
  // on the order of the corresponding highlights in the page. Use a barrier
  // closure to wait until all note highlights have been created in the page.
  base::RepeatingClosure barrier =
      base::BarrierClosure(notes_added_.size(), std::move(callback));
  for (const base::UnguessableToken& note_id : notes_added_) {
    const UserNote* note = service_->GetNoteModel(note_id);
    DCHECK(note);

    std::unique_ptr<UserNoteInstance> instance_unique =
        MakeNoteInstance(note, manager);
    manager->AddNoteInstance(std::move(instance_unique), barrier);
  }
}

std::unique_ptr<UserNoteInstance> FrameUserNoteChanges::MakeNoteInstance(
    const UserNote* note_model,
    UserNoteManager* manager) const {
  return UserNoteInstance::Create(note_model->GetSafeRef(), manager);
}

}  // namespace user_notes
