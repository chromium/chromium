// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/frame_user_note_changes.h"

#include "base/notreached.h"
#include "content/public/browser/render_frame_host.h"

namespace user_notes {

FrameUserNoteChanges::FrameUserNoteChanges(content::RenderFrameHost* rfh,
                                           const ChangeList& notes_added,
                                           const ChangeList& notes_modified,
                                           const ChangeList& notes_removed)
    : rfh_(rfh),
      notes_added_(notes_added),
      notes_modified_(notes_modified),
      notes_removed_(notes_removed) {
  DCHECK(!notes_added_.empty() || !notes_modified_.empty() ||
         !notes_removed_.empty());
}

FrameUserNoteChanges::FrameUserNoteChanges(content::RenderFrameHost* rfh,
                                           ChangeList&& notes_added,
                                           ChangeList&& notes_modified,
                                           ChangeList&& notes_removed)
    : rfh_(rfh),
      notes_added_(std::move(notes_added)),
      notes_modified_(std::move(notes_modified)),
      notes_removed_(std::move(notes_removed)) {
  DCHECK(!notes_added_.empty() || !notes_modified_.empty() ||
         !notes_removed_.empty());
}

FrameUserNoteChanges::FrameUserNoteChanges(FrameUserNoteChanges&& other) =
    default;

FrameUserNoteChanges::~FrameUserNoteChanges() = default;

void FrameUserNoteChanges::Apply() {
  NOTIMPLEMENTED();
}

}  // namespace user_notes
