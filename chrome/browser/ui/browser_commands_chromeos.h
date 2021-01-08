// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_COMMANDS_CHROMEOS_H_
#define CHROME_BROWSER_UI_BROWSER_COMMANDS_CHROMEOS_H_

class Browser;

// Moves |browser| from the active desk to the desk at |desk_index|.
void SendToDeskAtIndex(Browser* browser, int desk_index);

// Takes a screenshot of the entire desktop (not just the browser window).
void TakeScreenshot();

// Toggles whether |browser| is assigned to all desks.
void ToggleAssignedToAllDesks(Browser* browser);

#endif  // CHROME_BROWSER_UI_BROWSER_COMMANDS_CHROMEOS_H_
