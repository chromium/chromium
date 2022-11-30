-- Copyright 2010 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- creates 2 windows, one in normal mode and another in incognito mode.
tell application "Chromium"
  make new window
  make new window with properties {mode:"incognito"}
  count windows  -- count how many windows are currently open.
end tell
