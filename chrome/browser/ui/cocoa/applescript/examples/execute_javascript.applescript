-- Copyright 2010 The Chromium Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- This script execute a string of javascript code.
tell application "Chromium"
	tell tab 1 of window 1
		execute javascript "alert('Hello World')"
	end tell
end tell
