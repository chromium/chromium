// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_NOTES_USER_NOTES_CONTROLLER_H_
#define CHROME_BROWSER_UI_USER_NOTES_USER_NOTES_CONTROLLER_H_

class TabStripModel;
class Profile;

class UserNotesController {
 public:
  // Returns true if the user notes feature is available for the given profile.
  static bool IsUserNotesSupported(Profile* profile);

  // Switches to the tab, opens notes ui, and starts the note creation flow.
  static void SwitchTabsAndAddNote(TabStripModel* tab_strip, int tab_index);
};

#endif  // CHROME_BROWSER_UI_USER_NOTES_USER_NOTES_CONTROLLER_H_
