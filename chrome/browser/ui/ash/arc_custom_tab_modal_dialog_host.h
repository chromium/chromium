// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ARC_CUSTOM_TAB_MODAL_DIALOG_HOST_H_
#define CHROME_BROWSER_UI_ASH_ARC_CUSTOM_TAB_MODAL_DIALOG_HOST_H_

#include <memory>

#include "base/macros.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {
class ArcCustomTab;
}  // namespace ash

namespace contents {
class WebContents;
}  // namespace contents

namespace gfx {
class Point;
class Size;
}  // namespace gfx

namespace web_modal {
class ModalDialogHostObserver;
}  // namespace web_modal

// Implements a WebContentsModalDialogHost for an ARC Custom Tab. This allows a
// web contents modal dialog to be drawn in the ARC Custom Tab.
class ArcCustomTabModalDialogHost
    : public web_modal::WebContentsModalDialogHost,
      public web_modal::WebContentsModalDialogManagerDelegate {
 public:
  ArcCustomTabModalDialogHost(
      std::unique_ptr<ash::ArcCustomTab> custom_tab,
      std::unique_ptr<content::WebContents> web_contents);
  ~ArcCustomTabModalDialogHost() override = 0;

  // web_modal::WebContentsModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // web_modal::WebContentsModalDialogHost:
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

 protected:
  std::unique_ptr<ash::ArcCustomTab> custom_tab_;
  std::unique_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ArcCustomTabModalDialogHost);
};

#endif  // CHROME_BROWSER_UI_ASH_ARC_CUSTOM_TAB_MODAL_DIALOG_HOST_H_
