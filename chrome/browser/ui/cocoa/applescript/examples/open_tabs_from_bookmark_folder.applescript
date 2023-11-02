-- Copyright 2010 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

tell application "Chromium"
	set var to bookmark folder "New" of bookmarks bar
	-- Change the folder to whichever you want.
	repeat with i in (bookmark items of var)
		set u to URL of i
		tell window 1 to make new tab with properties {u}
	end repeat
end tell
