-- Copyright 2010 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

tell application "Chromium"
  tell tab 1 of window 1
    print  -- Prints the tab, prompts the user for location.
  end tell
  
  tell tab 1 of window 1
    save in "/Users/Foo/Documents/Google" as "only html"
    -- Saves the contents of the tab without the accompanying resources.
    
    save in "/Users/Foo/Documents/Google" as "complete html"
    -- Saves the contents of the tab with the accompanying resources.
    
    -- Note: both the |in| and |as| part are optional, without it user is
    -- prompted for one.
  end tell
  
  tell tab 1 of window 1
    view source  -- View the HTML of the tab in a new tab.
  end tell
end tell
