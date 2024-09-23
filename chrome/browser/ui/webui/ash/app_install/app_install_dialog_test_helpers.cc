// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog_test_helpers.h"

#include <optional>
#include <string>

#include "base/values.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/test/browser_test_utils.h"

namespace ash::app_install {

content::WebContents* GetWebContentsFromDialog() {
  ash::SystemWebDialogDelegate* dialog =
      ash::SystemWebDialogDelegate::FindInstance(
          chrome::kChromeUIAppInstallDialogURL);
  if (!dialog) {
    return nullptr;
  }
  content::WebUI* webui = dialog->GetWebUIForTest();
  if (!webui) {
    return nullptr;
  }
  return webui->GetWebContents();
}

std::string GetDialogTitle(content::WebContents* web_contents) {
  return content::EvalJs(web_contents, R"(
      document.querySelector('app-install-dialog').shadowRoot
              .querySelector('#title').textContent
    )")
      .ExtractString();
}

std::optional<std::string> GetDialogActionButton(
    content::WebContents* web_contents) {
  content::EvalJsResult result = content::EvalJs(web_contents, R"(
      const button = document.querySelector('app-install-dialog').shadowRoot
              .querySelector('.action-button');
      button.style.display === 'none' ? null : button.label;
    )");
  if (result == base::Value()) {
    return std::nullopt;
  }
  return result.ExtractString();
}

bool ClickDialogActionButton(content::WebContents* web_contents) {
  return content::ExecJs(web_contents, R"(
      document.querySelector('app-install-dialog').shadowRoot
              .querySelector('.action-button').click();
    )");
}

}  // namespace ash::app_install
