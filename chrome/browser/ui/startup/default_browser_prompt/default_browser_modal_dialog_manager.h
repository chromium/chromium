// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_MODAL_DIALOG_MANAGER_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_MODAL_DIALOG_MANAGER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_surface_manager.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"

namespace default_browser {

// The "Modal" entrypoint for the Windows Default Browser. This is the global
// manger for the modal dialog instances across all browser windows. This dialog
// displays a WebUI page to provide prompts that help users navigate OS settings
// to set Chrome as the default browser based the DefaultBrowserSetter
// behavior.
class DefaultBrowserModalDialogManager : public DefaultBrowserSurfaceManager {
 public:
  explicit DefaultBrowserModalDialogManager(bool use_settings_illustration);

  DefaultBrowserModalDialogManager(const DefaultBrowserModalDialogManager&) =
      delete;
  DefaultBrowserModalDialogManager& operator=(
      const DefaultBrowserModalDialogManager&) = delete;

  ~DefaultBrowserModalDialogManager() override;

  // DefaultBrowserSurfaceManager:
  default_browser::DefaultBrowserEntrypointType GetEntrypointType()
      const override;

 protected:
  // DefaultBrowserSurfaceManager:
  void ShowForBrowser(BrowserWindowInterface* browser) final;
  void CloseForBrowser(BrowserWindowInterface* browser) final;
  void CloseAllPromptInstances() final;

 private:
  void OnDialogWidgetCloseRequested(BrowserWindowInterface* browser,
                                    views::Widget::ClosedReason reason);

  const bool use_settings_illustration_;

  // A map of browser windows to the prompt modal widgets.
  std::map<BrowserWindowInterface*, std::unique_ptr<views::Widget>>
      dialog_widgets_;
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_MODAL_DIALOG_MANAGER_H_
