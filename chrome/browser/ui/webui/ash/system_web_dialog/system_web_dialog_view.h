// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SYSTEM_WEB_DIALOG_SYSTEM_WEB_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SYSTEM_WEB_DIALOG_SYSTEM_WEB_DIALOG_VIEW_H_

#include <memory>

#include "ui/views/controls/webview/web_dialog_view.h"

namespace ui {
class WebDialogDelegate;
}  // namespace ui

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace ash {

// WebDialogView for system Web UI dialogs, e.g. dialogs opened from the
// Ash system tray.
class SystemWebDialogView : public views::WebDialogView {
 public:
  SystemWebDialogView(content::BrowserContext* context,
                      ui::WebDialogDelegate* delegate,
                      std::unique_ptr<WebContentsHandler> handler,
                      content::WebContents* web_contents = nullptr);

  SystemWebDialogView(const SystemWebDialogView&) = delete;
  SystemWebDialogView& operator=(const SystemWebDialogView&) = delete;

  ~SystemWebDialogView() override = default;

  // views::ClientView:
  void UpdateWindowRoundedCorners(int corner_radius) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SYSTEM_WEB_DIALOG_SYSTEM_WEB_DIALOG_VIEW_H_
