// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SYSTEM_WEB_DIALOG_SYSTEM_WEB_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SYSTEM_WEB_DIALOG_SYSTEM_WEB_DIALOG_VIEW_H_

#include <memory>

#include "base/observer_list.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/controls/webview/web_dialog_view.h"

namespace gfx {
class Rect;
class RoundedCornersF;
}  // namespace gfx

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
class SystemWebDialogView
    : public views::WebDialogView,
      public web_modal::WebContentsModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost {
 public:
  SystemWebDialogView(content::BrowserContext* context,
                      ui::WebDialogDelegate* delegate,
                      std::unique_ptr<WebContentsHandler> handler,
                      content::WebContents* web_contents = nullptr);

  SystemWebDialogView(const SystemWebDialogView&) = delete;
  SystemWebDialogView& operator=(const SystemWebDialogView&) = delete;

  ~SystemWebDialogView() override;

  // views::ClientView:
  void UpdateWindowRoundedCorners(
      const gfx::RoundedCornersF& window_radii) override;

  // views::View:
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // views::WidgetDelegate:
  void WindowClosing() override;

  // web_modal::WebContentsModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost(
      content::WebContents* web_contents) override;
  bool IsWebContentsVisible(content::WebContents* web_contents) override;

  // web_modal::WebContentsModalDialogHost:
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

 private:
  void ResetModalDialogHost();

  bool modal_dialog_host_reset_ = false;
  base::ObserverList<web_modal::ModalDialogHostObserver>
      modal_dialog_host_observer_list_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SYSTEM_WEB_DIALOG_SYSTEM_WEB_DIALOG_VIEW_H_
