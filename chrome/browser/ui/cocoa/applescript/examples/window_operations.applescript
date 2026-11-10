-- Copyright 2011 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- Contains usage of common window operations.
tell application "Chromium"
  set aWindow to window 1

  get URL of active tab of aWindow  -- The URL currently being seen.

  set minimized of aWindow to true -- Minimizes a window.
  set minimized of aWindow to false -- Maximizes a window.

  -- Retrieves whether a window is in "normal" or "incognito" mode.
  get mode of aWindow

  set visible of aWindow to false  -- Hides a window.
  set visible of aWindow to true  -- UnHides a window.
end tell
