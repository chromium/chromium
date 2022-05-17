// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_instance.h"

#include "components/user_notes/browser/user_note_manager.h"
#include "components/user_notes/model/user_note_target.h"

namespace user_notes {

UserNoteInstance::UserNoteInstance(base::SafeRef<UserNote> model,
                                   UserNoteManager* parent_manager)
    : UserNoteInstance(model, parent_manager, gfx::Rect()) {}

UserNoteInstance::UserNoteInstance(base::SafeRef<UserNote> model,
                                   UserNoteManager* parent_manager,
                                   gfx::Rect rect)
    : model_(model),
      parent_manager_(parent_manager),
      rect_(rect),
      receiver_(this) {}

UserNoteInstance::~UserNoteInstance() = default;

bool UserNoteInstance::IsDetached() const {
  return is_initialized_ && rect_.IsEmpty() &&
         model_->target().type() == UserNoteTarget::TargetType::kPageText;
}

void UserNoteInstance::InitializeHighlightIfNeeded(base::OnceClosure callback) {
  DCHECK(!is_initialized_);

  if (model_->target().type() == UserNoteTarget::TargetType::kPage) {
    // Page-level notes are not associated with text in the page, so there is no
    // highlight to create on the renderer side.
    is_initialized_ = true;
    DCHECK(callback);
    std::move(callback).Run();
  } else {
    did_finish_attachment_callback_ = std::move(callback);
    InitializeHighlightInternal();
  }
}

void UserNoteInstance::DidFinishAttachment(const gfx::Rect& rect) {
  is_initialized_ = true;
  rect_ = rect;

  DCHECK(did_finish_attachment_callback_);
  std::move(did_finish_attachment_callback_).Run();
}

void UserNoteInstance::OnNoteDetached() {
  rect_ = gfx::Rect();
  DCHECK(IsDetached());

  // TODO(gujen): Notify the service so it can invalidate the UI.
}

void UserNoteInstance::InitializeHighlightInternal() {
  DCHECK_EQ(model_->target().type(), UserNoteTarget::TargetType::kPageText);

  parent_manager_->note_agent_container()->CreateAgent(
      receiver_.BindNewPipeAndPassRemote(), agent_.BindNewPipeAndPassReceiver(),
      blink::mojom::AnnotationType::kUserNote, model_->target().selector());

  // Set a disconnect handler because the renderer can close the pipe at any
  // moment to signal that the highlight has been removed from the page. It's ok
  // to use base::unretained here because the note instances are always
  // destroyed before the manager (the manager's destructor explicitly destroys
  // them).
  agent_.set_disconnect_handler(
      base::BindOnce(&UserNoteManager::RemoveNote,
                     base::Unretained(parent_manager_), model_->id()));
}

}  // namespace user_notes
