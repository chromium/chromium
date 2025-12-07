// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_MODAL_DIALOG_HOST_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_MODAL_DIALOG_HOST_H_

#include "base/observer_list.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"

class WebUIBrowserWindow;

class WebUIBrowserModalDialogHost
    : public web_modal::WebContentsModalDialogHost {
 public:
  explicit WebUIBrowserModalDialogHost(WebUIBrowserWindow* window);
  ~WebUIBrowserModalDialogHost() override;

  void NotifyPositionRequiresUpdate();

  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  bool ShouldActivateDialog() const override;
  gfx::Size GetMaximumDialogSize() override;

  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

 private:
  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked observers_;

  raw_ptr<WebUIBrowserWindow> browser_window_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_MODAL_DIALOG_HOST_H_
