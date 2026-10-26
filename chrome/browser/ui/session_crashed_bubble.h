// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SESSION_CRASHED_BUBBLE_H_
#define CHROME_BROWSER_UI_SESSION_CRASHED_BUBBLE_H_

class BrowserWindowInterface;

// Shows the session crashed bubble if the current profile is neither an
// Incognito nor a Guest profile. It also offers the option to enable metrics
// reporting if it's not already enabled.
// Note: It is the caller's responsibility to determine if the previous
// session ended with a crash.
void ShowSessionCrashedBubbleIfNotOffTheRecordProfile(
    BrowserWindowInterface* browser,
    bool skip_tab_checking);

#endif  // CHROME_BROWSER_UI_SESSION_CRASHED_BUBBLE_H_
