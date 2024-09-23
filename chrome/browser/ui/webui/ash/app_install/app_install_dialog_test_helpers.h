// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_DIALOG_TEST_HELPERS_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_DIALOG_TEST_HELPERS_H_

#include <optional>
#include <string>

namespace content {
class WebContents;
}

namespace ash::app_install {

// Returns the WebContents currently being displayed in an App Install Dialog,
// or nullptr if there is no dialog open.
content::WebContents* GetWebContentsFromDialog();

std::string GetDialogTitle(content::WebContents* web_contents);

std::optional<std::string> GetDialogActionButton(
    content::WebContents* web_contents);

[[nodiscard]] bool ClickDialogActionButton(content::WebContents* web_contents);

}  // namespace ash::app_install

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_APP_INSTALL_APP_INSTALL_DIALOG_TEST_HELPERS_H_
