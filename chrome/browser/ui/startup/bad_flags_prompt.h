// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_BAD_FLAGS_PROMPT_H_
#define CHROME_BROWSER_UI_STARTUP_BAD_FLAGS_PROMPT_H_

namespace content {
class WebContents;
}

// Shows a warning notification in |web_contents| that the app was run with
// dangerous command line flags or dangerous flags in about:flags.
// On Android, this method doesn't check any flags which are not available in
// about:flags.
void ShowBadFlagsPrompt(content::WebContents* web_contents);

// Shows a warning about a specific flag.  Exposed publicly only for testing;
// should otherwise be used only by ShowBadFlagsPrompt().
void ShowBadFlagsInfoBar(content::WebContents* web_contents,
                         int message_id,
                         const char* flag);

// Shows a warning dialog if the originally specified user data dir was invalid.
void MaybeShowInvalidUserDataDirWarningDialog();

#endif  // CHROME_BROWSER_UI_STARTUP_BAD_FLAGS_PROMPT_H_
