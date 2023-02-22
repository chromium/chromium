// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_notes/user_notes_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/user_notes/browser/user_note_manager.h"
#include "components/user_notes/interfaces/user_notes_ui.h"
#include "components/user_notes/user_notes_features.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

// static
bool UserNotesController::IsUserNotesSupported(Profile* profile) {
  return user_notes::IsUserNotesEnabled() &&
         base::FeatureList::IsEnabled(power_bookmarks::kPowerBookmarkBackend) &&
         !profile->IsGuestSession();
}

// static
bool UserNotesController::IsUserNotesSupported(
    content::WebContents* web_contents) {
  // Use the last committed url, this matches what is done in
  // UserNotesPageHandler for getting and creating notes.
  return !search::IsNTPURL(web_contents->GetLastCommittedURL());
}

// static
void UserNotesController::InitiateNoteCreationForTab(Browser* browser,
                                                     int tab_index) {
  user_notes::UserNotesUI* notes_ui = static_cast<user_notes::UserNotesUI*>(
      browser->GetUserData(user_notes::UserNotesUI::UserDataKey()));
  DCHECK(notes_ui);
  notes_ui->SwitchTabsAndStartNoteCreation(tab_index);
}

// static
void UserNotesController::InitiateNoteCreationForCurrentTab(Browser* browser) {
  user_notes::UserNotesUI* notes_ui = static_cast<user_notes::UserNotesUI*>(
      browser->GetUserData(user_notes::UserNotesUI::UserDataKey()));
  DCHECK(notes_ui);
  notes_ui->StartNoteCreation();
}
