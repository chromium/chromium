// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_instance.h"

namespace user_notes {

UserNoteInstance::UserNoteInstance(base::SafeRef<UserNote> model)
    : model_(model) {}

UserNoteInstance::~UserNoteInstance() = default;

void UserNoteInstance::InitializeHighlightIfNeeded(base::OnceClosure callback) {
  if (model_->target().type() == UserNoteTarget::TargetType::kPage) {
    // Page-level notes are not associated with text in the page, so there is
    // no highlight to create on the renderer side.
    if (callback) {
      std::move(callback).Run();
    }
  } else {
    did_finish_attachment_callback_ = std::move(callback);

    // TODO(gujen): Implement the async initialization of highlights in the
    // renderer. For now, ignore the async part and calling the attachment
    // handler directly (it will eventually be called by the renderer side).
    DidFinishAttachment();
  }
}

void UserNoteInstance::DidFinishAttachment() {
  // TODO(gujen): Add the rect arg to the method signature, assign it to a
  // class member and detect whether the note is in an attached state or not.
  DCHECK(did_finish_attachment_callback_);
  std::move(did_finish_attachment_callback_).Run();
}

}  // namespace user_notes
