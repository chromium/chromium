-- Copyright 2010 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- This script deletes all the items within a bookmark folder.
tell application "Chromium"
  set var to bookmark folder "New" of bookmarks bar
  -- Change the folder to whichever you want.
  tell var
    delete every bookmark item
    delete every bookmark folder
  end tell
end tell
