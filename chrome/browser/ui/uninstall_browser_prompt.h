// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_UNINSTALL_BROWSER_PROMPT_H_
#define CHROME_BROWSER_UI_UNINSTALL_BROWSER_PROMPT_H_

// Asks user for uninstall confirmation and returns one of these values:
// content::RESULT_CODE_NORMAL_EXIT,
// chrome::RESULT_CODE_UNINSTALL_DELETE_PROFILE or
// chrome::RESULT_CODE_UNINSTALL_USER_CANCEL.
int ShowUninstallBrowserPrompt();

#endif  // CHROME_BROWSER_UI_UNINSTALL_BROWSER_PROMPT_H_
