-- Copyright 2010 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- This script copies the HTML of a tab to a TextEdit document.
tell application "Chromium"
	tell tab 1 of window 1 to view source
	repeat while (loading of tab 2 of window 1)
	end repeat
	tell tab 2 of window 1 to select all
	tell tab 2 of window 1 to copy selection
end tell

tell application "TextEdit"
	set text of document 1 to the clipboard
end tell
