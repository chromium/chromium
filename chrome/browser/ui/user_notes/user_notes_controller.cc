// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_notes/user_notes_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/user_notes/browser/user_note_manager.h"
#include "components/user_notes/user_notes_features.h"
#include "content/public/browser/web_contents.h"

// static
bool UserNotesController::IsUserNotesSupported(Profile* profile) {
  return user_notes::IsUserNotesEnabled() &&
         base::FeatureList::IsEnabled(features::kUnifiedSidePanel) &&
         !profile->IsGuestSession();
}

// static
void UserNotesController::SwitchTabsAndAddNote(TabStripModel* tab_strip,
                                               int tab_index) {
  auto* notes_manager = user_notes::UserNoteManager::GetForPage(
      tab_strip->GetWebContentsAt(tab_index)->GetPrimaryPage());
  if (!notes_manager)
    return;
  tab_strip->ActivateTabAt(
      tab_index, TabStripUserGestureDetails(
                     TabStripUserGestureDetails::GestureType::kOther));
  notes_manager->OnAddNoteRequested(
      tab_strip->GetWebContentsAt(tab_index)->GetPrimaryMainFrame(),
      /*has_selected_text=*/false);
}
