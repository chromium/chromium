-- Copyright 2010 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

tell application "Chromium"
  tell window 1
    -- Creates a new tab and navigates to a particular URL.
    make new tab with properties {URL:"http://google.com"}

    -- Duplicate a tab.
    set theLocation to URL of tab 2
    make new tab with properties {URL:theLocation}
  end tell
end tell
