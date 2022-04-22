// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_notes/user_note_service_delegate_impl.h"

#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_notes/interfaces/user_notes_ui.h"
#include "content/public/browser/render_frame_host.h"

namespace user_notes {

UserNoteServiceDelegateImpl::UserNoteServiceDelegateImpl(Profile* profile)
    : profile_(profile) {}

UserNoteServiceDelegateImpl::~UserNoteServiceDelegateImpl() = default;

std::vector<content::RenderFrameHost*>
UserNoteServiceDelegateImpl::GetAllFramesForUserNotes() {
  // TODO(crbug.com/1313967): For now, this only looks at the primary main frame
  // of open tabs, since notes will initially only be supported there. When / if
  // User Notes are supported in AMP viewers and subframes in general, this will
  // need to walk the full frame tree of every open tab for the profile.
  // TODO(gujen): finish implementation.
  NOTIMPLEMENTED();
  return std::vector<content::RenderFrameHost*>();
}

UserNotesUI* UserNoteServiceDelegateImpl::GetUICoordinatorForFrame(
    const content::RenderFrameHost* rfh) {
  // TODO(gujen): finish implementation.
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace user_notes
