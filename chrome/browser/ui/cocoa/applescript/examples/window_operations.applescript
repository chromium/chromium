-- Copyright 2011 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- Contains usage of common window operations.
tell application "Chromium"
  get URL of active tab of window 1  -- The URL currently being seen.
  
  set minimized of window 1 to true  -- Minimizes a window.
  set minimized of window 1 to false  -- Maximizes a window.
  
  get mode of window 1
  -- Checks if a window is in |normal mode| or |incognito mode|
  
  set visible of window 1 to true  -- Hides a window.
  set visible of window 1 to false  -- UnHides a window.
  
  -- Open multiple tabs.
  set active tab index of window 1 to 2  -- Selects the second tab.
  
  -- Enter/exit presentation mode
  if window 1 is not presenting
    tell window 1 to enter presentation mode
  end if
  if window 1 is presenting
    tell window 1 to exit presentation mode
  end if
  
end tell
