// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"

namespace webui_browser {

bool IsWebUIBrowserEnabled() {
  return base::FeatureList::IsEnabled(features::kWebium);
}

bool IsBrowserUIWebContents(content::WebContents* web_contents) {
  if (IsWebUIBrowserEnabled() &&
      WebUIBrowserWindow::FromWebShellWebContents(web_contents)) {
    return true;
  }

  if (web_contents->GetWebUI() && web_contents->GetWebUI()->GetController() &&
      web_contents->GetWebUI()->GetController()->GetDisplayDisposition() ==
          content::WebUIController::DisplayDisposition::kUIElement) {
    return true;
  }

  return false;
}

}  // namespace webui_browser
