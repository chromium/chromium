// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/content/content_infobar_manager.h"

#include "base/command_line.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "ui/base/page_transition_types.h"

namespace infobars {

// static
InfoBarDelegate::NavigationDetails
ContentInfoBarManager::NavigationDetailsFromLoadCommittedDetails(
    const content::LoadCommittedDetails& details) {
  InfoBarDelegate::NavigationDetails navigation_details;
  navigation_details.entry_id = details.entry->GetUniqueID();
  navigation_details.is_navigation_to_different_page =
      details.is_navigation_to_different_page();
  navigation_details.did_replace_entry = details.did_replace_entry;
  const ui::PageTransition transition = details.entry->GetTransitionType();
  navigation_details.is_reload =
      ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD);
  navigation_details.is_redirect = ui::PageTransitionIsRedirect(transition);
  return navigation_details;
}

// static
content::WebContents* ContentInfoBarManager::WebContentsFromInfoBar(
    InfoBar* infobar) {
  if (!infobar || !infobar->owner())
    return nullptr;
  ContentInfoBarManager* infobar_manager =
      static_cast<ContentInfoBarManager*>(infobar->owner());
  return infobar_manager->web_contents();
}

ContentInfoBarManager::ContentInfoBarManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ContentInfoBarManager>(*web_contents) {
  DCHECK(web_contents);
  // Infobar animations cause viewport resizes. Disable them for automated
  // tests, since they could lead to flakiness.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAutomation))
    set_animations_enabled(false);
}

ContentInfoBarManager::~ContentInfoBarManager() {
  ShutDown();
}

int ContentInfoBarManager::GetActiveEntryID() {
  content::NavigationEntry* active_entry =
      web_contents()->GetController().GetActiveEntry();
  return active_entry ? active_entry->GetUniqueID() : 0;
}

void ContentInfoBarManager::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  RemoveAllInfoBars(true);
}

void ContentInfoBarManager::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  ignore_next_reload_ = false;
}

void ContentInfoBarManager::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  const bool ignore =
      ignore_next_reload_ &&
      ui::PageTransitionCoreTypeIs(load_details.entry->GetTransitionType(),
                                   ui::PAGE_TRANSITION_RELOAD);
  ignore_next_reload_ = false;
  if (!ignore)
    OnNavigation(NavigationDetailsFromLoadCommittedDetails(load_details));
}

void ContentInfoBarManager::WebContentsDestroyed() {
  // The WebContents is going away; be aggressively paranoid and delete
  // |this| lest other parts of the system attempt to add infobars or use
  // this object otherwise during the destruction.
  // TODO(blundell): This operation seems unnecessary as detailed in the
  // conversation on
  // https://chromium-review.googlesource.com/c/chromium/src/+/2859170/7 .
  // Look at removing it.
  web_contents()->RemoveUserData(UserDataKey());
  // That was the equivalent of "delete this". This object is now destroyed;
  // returning from this function is the only safe thing to do.
}

void ContentInfoBarManager::OpenURL(const GURL& url,
                                    WindowOpenDisposition disposition) {
  // A normal user click on an infobar URL will result in a CURRENT_TAB
  // disposition; turn that into a NEW_FOREGROUND_TAB so that we don't end up
  // smashing the page the user is looking at.
  web_contents()->OpenURL(
      content::OpenURLParams(url, content::Referrer(),
                             (disposition == WindowOpenDisposition::CURRENT_TAB)
                                 ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                 : disposition,
                             ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContentInfoBarManager);

}  // namespace infobars
