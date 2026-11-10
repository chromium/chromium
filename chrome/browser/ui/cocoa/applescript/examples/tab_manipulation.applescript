-- Copyright 2010 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- Contains some common tab manipulation commands.
tell application "Chromium"
  -- Create a new tab and navigate to a particular URL.
  tell window 1 to make new tab with properties {URL:"http://google.com"}

  -- Select the previous tab.
  set activeIndex to active tab index of window 1
  set active tab index of window 1 to (activeIndex - 1)

  -- Select the next tab.
  set activeIndex to active tab index of window 1
  set active tab index of window 1 to (activeIndex + 1)

  -- Get the tab's title.
  get name of tab 1 of window 1

  -- Get the tab's URL.
  get URL of tab 1 of window 1

  -- Check if a tab is loading.
  get loading of tab 1 of window 1

  -- Common edit/manipulation commands.
  tell tab 1 of window 1
    undo

    redo

    -- Cut a piece of text and place it on the system clipboard.
    cut selection

    -- Copy a piece of text and place it on the system clipboard.
    copy selection

    -- Paste text from the system clipboard.
    paste selection

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
