// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_NOTES_USER_NOTES_CONTROLLER_H_
#define CHROME_BROWSER_UI_USER_NOTES_USER_NOTES_CONTROLLER_H_

class Browser;
class Profile;

namespace content {
class WebContents;
}

class UserNotesController {
 public:
  // Returns true if the user notes feature is available for the given profile.
  static bool IsUserNotesSupported(Profile* profile);

  // Returns true if notes can be taken for the given web contents.
  static bool IsUserNotesSupported(content::WebContents* web_contents);

  // Switches to the tab, opens notes ui, and starts the note creation flow.
  static void InitiateNoteCreationForTab(Browser* browser, int tab_index);

  // Opens notes ui and starts the note creation flow.
  static void InitiateNoteCreationForCurrentTab(Browser* browser);
};

#endif  // CHROME_BROWSER_UI_USER_NOTES_USER_NOTES_CONTROLLER_H_
