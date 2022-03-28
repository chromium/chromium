// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_notes/user_note_service_delegate_impl.h"

#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_notes/interfaces/user_notes_ui.h"
#include "content/public/browser/web_contents.h"

namespace user_notes {

UserNoteServiceDelegateImpl::UserNoteServiceDelegateImpl(Profile* profile)
    : profile_(profile) {}

UserNoteServiceDelegateImpl::~UserNoteServiceDelegateImpl() = default;

std::vector<content::WebContents*>
UserNoteServiceDelegateImpl::GetAllWebContents() {
  // TODO(gujen): finish implementation.
  NOTIMPLEMENTED();
  return std::vector<content::WebContents*>();
}

UserNotesUI* UserNoteServiceDelegateImpl::GetUICoordinatorForWebContents(
    const content::WebContents* wc) {
  // TODO(gujen): finish implementation.
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace user_notes
