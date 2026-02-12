// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_BUBBLE_DIALOG_MANAGER_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_BUBBLE_DIALOG_MANAGER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_surface_manager.h"

namespace views {
class Widget;
}

class BrowserWindowInterface;

// Manages the default browser dialog, ensuring it is shown on all appropriate
// browser windows and handling user interactions.
class DefaultBrowserBubbleDialogManager : public BrowserCollectionObserver,
                                          public DefaultBrowserSurfaceManager {
 public:
  DefaultBrowserBubbleDialogManager();
  ~DefaultBrowserBubbleDialogManager() override;

  DefaultBrowserBubbleDialogManager(const DefaultBrowserBubbleDialogManager&) =
      delete;
  DefaultBrowserBubbleDialogManager& operator=(
      const DefaultBrowserBubbleDialogManager&) = delete;

  // DefaultBrowserSurfaceManager:
  void Show(
      std::unique_ptr<default_browser::DefaultBrowserController> controller,
      bool can_pin_to_taskbar) override;
  void CloseAll() override;
  default_browser::DefaultBrowserEntrypointType GetEntrypointType()
      const override;

 private:
  void OnAccept();
  void OnDismiss();

  // BrowserCollectionObserver
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

  bool can_pin_to_taskbar_ = false;

  std::unique_ptr<default_browser::DefaultBrowserController>
      default_browser_controller_;

  // A map of browser windows to the dialog widgets that are shown for them.
  // The widget is owned by the browser window.
  std::map<raw_ptr<BrowserWindowInterface>, std::unique_ptr<views::Widget>>
      dialog_widgets_;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_BUBBLE_DIALOG_MANAGER_H_
