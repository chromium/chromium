// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_instance.h"

#include "base/trace_event/typed_macros.h"
#include "components/user_notes/browser/user_note_manager.h"
#include "components/user_notes/model/user_note_target.h"

namespace user_notes {

UserNoteInstance::UserNoteInstance(base::SafeRef<UserNote> model,
                                   UserNoteManager* parent_manager,
                                   PassKey pass_key)
    : model_(std::move(model)),
      parent_manager_(parent_manager),
      receiver_(this) {}

UserNoteInstance::UserNoteInstance(base::SafeRef<UserNote> model,
                                   UserNoteManager* parent_manager)
    : UserNoteInstance(std::move(model), parent_manager, PassKey()) {}

// static
std::unique_ptr<UserNoteInstance> UserNoteInstance::Create(
    base::SafeRef<UserNote> model,
    UserNoteManager* parent_manager) {
  return std::make_unique<UserNoteInstance>(std::move(model), parent_manager,
                                            PassKey());
}

UserNoteInstance::~UserNoteInstance() = default;

bool UserNoteInstance::IsDetached() const {
  return finished_attachment_ && rect_.IsEmpty() &&
         model_->target().type() == UserNoteTarget::TargetType::kPageText;
}

void UserNoteInstance::BindToHighlight(
    mojo::PendingReceiver<blink::mojom::AnnotationAgentHost> host_receiver,
    mojo::PendingRemote<blink::mojom::AnnotationAgent> agent_remote,
    AttachmentFinishedCallback callback) {
  DCHECK_EQ(model_->target().type(), UserNoteTarget::TargetType::kPageText);
  DCHECK(!agent_.is_bound());
  DCHECK(!receiver_.is_bound());
  DCHECK(agent_remote.is_valid());
  DCHECK(host_receiver.is_valid());

  did_finish_attachment_callback_ = std::move(callback);
  agent_.Bind(std::move(agent_remote));
  // base::Unretained since note instances are always destroyed before the
  // manager (the manager's destructor explicitly destroys them).
  agent_.set_disconnect_handler(
      base::BindOnce(&UserNoteManager::RemoveNote,
                     base::Unretained(parent_manager_), model_->id()));

  receiver_.Bind(std::move(host_receiver));
}

void UserNoteInstance::InitializeHighlightIfNeeded(
    AttachmentFinishedCallback callback) {
  TRACE_EVENT("browser", "UserNoteInstance::InitializeHighlightIfNeeded", "id",
              model_->id());
  // If the UserNoteInstance was instantiated to create a new note, the
  // highlight will already be initialized in the renderer. In this case,
  // BindToHighlight must already have been called and so a `callback` must not
  // be passed to this method since a callback was already passed in
  // BindToHighlight.
  if (agent_.is_bound()) {
    TRACE_EVENT_INSTANT("browser", "Already bound");
    DCHECK(receiver_.is_bound());
    DCHECK(!callback);
    DCHECK_EQ(model_->target().type(), UserNoteTarget::TargetType::kPageText);
    DCHECK_EQ(finished_attachment_, did_finish_attachment_callback_.is_null());
    return;
  }

  DCHECK(callback);

  switch (model_->target().type()) {
    case UserNoteTarget::TargetType::kPage: {
      // Page-level notes are not associated with text in the page, so there is
      // no highlight to create on the renderer side. Also, this instance may
      // already have been initialized as part of generating a new note. In
      // these cases, do nothing.
      TRACE_EVENT_INSTANT("browser", "Page Note");
      std::move(callback).Run();
    } break;
    case UserNoteTarget::TargetType::kPageText: {
      DCHECK(!did_finish_attachment_callback_);
      did_finish_attachment_callback_ = std::move(callback);
      InitializeHighlightInternal();
    } break;
  }
}

void UserNoteInstance::OnNoteSelected() {
  if (!agent_)
    return;
  agent_->ScrollIntoView();
}

void UserNoteInstance::DidFinishAttachment(const gfx::Rect& rect) {
  TRACE_EVENT("browser", "UserNoteInstance::DidFinishAttachment", "id",
              model_->id(), "rect", rect.ToString());
  finished_attachment_ = true;
  rect_ = rect;

  if (did_finish_attachment_callback_)
    std::move(did_finish_attachment_callback_).Run();
}

void UserNoteInstance::OnWebHighlightFocused() {
  parent_manager_->OnWebHighlightFocused(model_->id());
}

void UserNoteInstance::OnNoteDetached() {
  rect_ = gfx::Rect();
  DCHECK(IsDetached());

  // TODO(gujen): Notify the service so it can invalidate the UI.
}

void UserNoteInstance::InitializeHighlightInternal() {
  TRACE_EVENT("browser", "UserNoteInstance::InitializeHighlightInternal");
  DCHECK_EQ(model_->target().type(), UserNoteTarget::TargetType::kPageText);

  parent_manager_->note_agent_container()->CreateAgent(
      receiver_.BindNewPipeAndPassRemote(), agent_.BindNewPipeAndPassReceiver(),
      blink::mojom::AnnotationType::kUserNote, model_->target().selector());

  // Set a disconnect handler because the renderer can close the pipe at any
  // moment to signal that the highlight has been removed from the page. It's ok
  // to use base::Unretained here because the note instances are always
  // destroyed before the manager (the manager's destructor explicitly destroys
  // them).
  agent_.set_disconnect_handler(
      base::BindOnce(&UserNoteManager::RemoveNote,
                     base::Unretained(parent_manager_), model_->id()));
}

}  // namespace user_notes
