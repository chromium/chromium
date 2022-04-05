// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_notes/user_notes_tab_helper.h"

#include "chrome/browser/user_notes/user_note_service_factory.h"
#include "components/user_notes/browser/user_note_service.h"
#include "components/user_notes/browser/user_notes_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace user_notes {

UserNotesTabHelper::UserNotesTabHelper(content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<UserNotesTabHelper>(*web_contents) {}

UserNotesTabHelper::~UserNotesTabHelper() = default;

void UserNotesTabHelper::PrimaryPageChanged(content::Page& page) {
  if (!UserNotesManager::GetForPage(page)) {
    UserNotesManager::CreateForPage(page, service()->GetSafeRef());
  }
}

void UserNotesTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Note: `DidFinishNavigation` is needed in addition to `PrimaryPageChanged`
  // because of non-main frame navigations.
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  service()->OnFrameNavigated(navigation_handle->GetRenderFrameHost());
}

UserNoteService* UserNotesTabHelper::service() const {
  return UserNoteServiceFactory::GetForContext(
      web_contents()->GetBrowserContext());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(UserNotesTabHelper);

}  // namespace user_notes
