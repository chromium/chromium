// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_CONTAINER_H_

#include "base/functional/callback_forward.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

// Shows the chrome app information in a native dialog box.
void ShowAppInfoInNativeDialog(content::WebContents* web_contents,
                               Profile* profile,
                               const extensions::Extension* app,
                               base::OnceClosure close_callback);

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_CONTAINER_H_
