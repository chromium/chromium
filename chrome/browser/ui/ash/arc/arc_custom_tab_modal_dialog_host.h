// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ARC_ARC_CUSTOM_TAB_MODAL_DIALOG_HOST_H_
#define CHROME_BROWSER_UI_ASH_ARC_ARC_CUSTOM_TAB_MODAL_DIALOG_HOST_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/native_widget_types.h"

namespace arc {
class CustomTab;
}  // namespace arc

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
    : public content::WebContentsObserver,
      public web_modal::WebContentsModalDialogHost,
      public web_modal::WebContentsModalDialogManagerDelegate {
 public:
  ArcCustomTabModalDialogHost(std::unique_ptr<arc::CustomTab> custom_tab,
                              content::WebContents* web_contents);

  ArcCustomTabModalDialogHost(const ArcCustomTabModalDialogHost&) = delete;
  ArcCustomTabModalDialogHost& operator=(const ArcCustomTabModalDialogHost&) =
      delete;

  ~ArcCustomTabModalDialogHost() override = 0;

  // content::WebContentsObserver:
  void PrimaryMainFrameWasResized(bool width_changed) override;

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
  std::unique_ptr<arc::CustomTab> custom_tab_;
  raw_ptr<content::WebContents> web_contents_;

 private:
  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      observer_list_;
};

#endif  // CHROME_BROWSER_UI_ASH_ARC_ARC_CUSTOM_TAB_MODAL_DIALOG_HOST_H_
