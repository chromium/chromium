// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_notes_change.h"

#include "base/notreached.h"
#include "content/public/browser/render_frame_host.h"

namespace user_notes {

UserNotesChange::UserNotesChange(content::RenderFrameHost* rfh,
                                 const ChangeList& new_notes,
                                 const ChangeList& modified_notes,
                                 const ChangeList& deleted_notes)
    : rfh_(rfh),
      new_notes_(new_notes),
      modified_notes_(modified_notes),
      deleted_notes_(deleted_notes) {}

UserNotesChange::UserNotesChange(content::RenderFrameHost* rfh,
                                 ChangeList&& new_notes,
                                 ChangeList&& modified_notes,
                                 ChangeList&& deleted_notes)
    : rfh_(rfh),
      new_notes_(std::move(new_notes)),
      modified_notes_(std::move(modified_notes)),
      deleted_notes_(std::move(deleted_notes)) {}

UserNotesChange::~UserNotesChange() = default;

void UserNotesChange::Apply() {
  NOTIMPLEMENTED();
}

}  // namespace user_notes
