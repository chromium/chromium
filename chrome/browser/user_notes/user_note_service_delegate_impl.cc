// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_notes/user_note_service_delegate_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/user_notes/interfaces/user_notes_ui.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace user_notes {

namespace {

Browser* GetBrowserFromRenderFrameHost(const content::RenderFrameHost* rfh) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          const_cast<content::RenderFrameHost*>(rfh));

  if (!web_contents) {
    return nullptr;
  }

  return chrome::FindBrowserWithWebContents(web_contents);
}

}  // namespace

UserNoteServiceDelegateImpl::UserNoteServiceDelegateImpl(Profile* profile)
    : profile_(profile) {}

UserNoteServiceDelegateImpl::~UserNoteServiceDelegateImpl() = default;

std::vector<content::RenderFrameHost*>
UserNoteServiceDelegateImpl::GetAllFramesForUserNotes() {
  // TODO(crbug.com/1313967): This returns only the primary main frame of open
  // tabs, since User Notes are only supported in the primary main frame for
  // now. When / if User Notes are supported in AMP viewers and subframes in
  // general, this will need to walk the full frame tree of every open tab for
  // the current profile and add each frame to the result, not just the root
  // frame.
  const std::vector<Browser*>& browsers =
      chrome::FindAllTabbedBrowsersWithProfile(profile_);
  std::vector<content::RenderFrameHost*> results;

  for (Browser* browser : browsers) {
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      results.emplace_back(
          tab_strip_model->GetWebContentsAt(i)->GetPrimaryMainFrame());
    }
  }

  return results;
}

UserNotesUI* UserNoteServiceDelegateImpl::GetUICoordinatorForFrame(
    const content::RenderFrameHost* rfh) {
  Browser* browser = GetBrowserFromRenderFrameHost(rfh);
  if (!browser) {
    return nullptr;
  }

  return static_cast<UserNotesUI*>(
      browser->GetUserData(UserNotesUI::UserDataKey()));
}

bool UserNoteServiceDelegateImpl::IsFrameInActiveTab(
    const content::RenderFrameHost* rfh) {
  Browser* browser = GetBrowserFromRenderFrameHost(rfh);
  if (!browser) {
    return false;
  }

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  content::WebContents* active_web_contents =
      tab_strip_model->GetActiveWebContents();

  if (active_web_contents) {
    return active_web_contents->GetPrimaryMainFrame() ==
           const_cast<content::RenderFrameHost*>(rfh)->GetMainFrame();
  } else {
    return false;
  }
}

}  // namespace user_notes
