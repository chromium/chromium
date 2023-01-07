-- Copyright 2010 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- Contains some common tab manipulation commands.
tell application "Chromium"
  tell window 1 to make new tab with properties {URL:"http://google.com"}
  -- create a new tab and navigate to a particular URL.
  
  set var to active tab index of window 1
  set active tab index of window 1 to (var - 1)  -- Select the previous tab.
  
  set var to active tab index of window 1
  set active tab index of window 1 to (var + 1)  -- Select the next tab.
  
  get title of tab 1 of window 1  -- Get the URL that the user can see.
  
  get loading of tab 1 of window 1  -- Check if a tab is loading.
  
  -- Common edit/manipulation commands.
  tell tab 1 of window 1
    undo
    
    redo
    
    cut selection  -- Cut a piece of text and place it on the system clipboard.
    
    copy selection  -- Copy a piece of text and place it on the system clipboard.
    
    paste selection  -- Paste a text from the system clipboard.
    
    select all
  end tell
  
  -- Common navigation commands.
  tell tab 1 of window 1
    go back
    
    go forward
    
    reload
    
    stop
  end tell
end tell
