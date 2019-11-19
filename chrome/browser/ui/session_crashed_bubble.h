// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SESSION_CRASHED_BUBBLE_H_
#define CHROME_BROWSER_UI_SESSION_CRASHED_BUBBLE_H_

#include "base/macros.h"

class Browser;

// Base class for a session restore request bubble, to be displayed when the
// previous session has crashed. It also presents an option to enable metrics
// reporting, if it not enabled already.
class SessionCrashedBubble {
 public:
  // Shows the session crashed bubble if the current profile is neither an
  // Incognito nor a Guest profile. It also offers the option to enable metrics
  // reporting if it's not already enabled.
  // Note: It is the caller's responsibility to determine if the previous
  // session ended with a crash.
  static void ShowIfNotOffTheRecordProfile(Browser* browser);

  virtual ~SessionCrashedBubble() {}

 protected:
  SessionCrashedBubble() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionCrashedBubble);
};

#endif  // CHROME_BROWSER_UI_SESSION_CRASHED_BUBBLE_H_
