-- Copyright 2010 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- This script bookmarks the currently open tabs of a window.
tell application "Chromium"
	set url_list to {}
	set title_list to {}
	tell window 1
		repeat with i from 1 to (count tabs)
			set end of url_list to (URL of tab i)
			set end of title_list to (title of tab i)
		end repeat
	end tell
	tell bookmarks bar
		set var to make new bookmark folder with properties {title:"New"}
		tell var
			repeat with i from 1 to (count url_list)
				make new bookmark item with properties {URL:(item i of url_list), title:(item i of title_list)}
			end repeat
		end tell
	end tell
end tell