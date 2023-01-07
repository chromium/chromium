// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_DIRECTORY_ACCESS_CONFIRMATION_DIALOG_H_
#define CHROME_BROWSER_UI_APPS_DIRECTORY_ACCESS_CONFIRMATION_DIALOG_H_

#include <string>

#include "base/functional/callback_forward.h"

namespace content {
class WebContents;
}

void CreateDirectoryAccessConfirmationDialog(bool writable,
                                             const std::u16string& app_name,
                                             content::WebContents* web_contents,
                                             base::OnceClosure on_accept,
                                             base::OnceClosure on_cancel);

#endif  // CHROME_BROWSER_UI_APPS_DIRECTORY_ACCESS_CONFIRMATION_DIALOG_H_
