// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_BUBBLE_DIALOG_MANAGER_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_BUBBLE_DIALOG_MANAGER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

namespace default_browser {
class DefaultBrowserController;
}

namespace views {
class Widget;
}

class BrowserWindowInterface;

// Manages the default browser dialog, ensuring it is shown on all appropriate
// browser windows and handling user interactions.
class DefaultBrowserBubbleDialogManager : public BrowserCollectionObserver {
 public:
  DefaultBrowserBubbleDialogManager();
  ~DefaultBrowserBubbleDialogManager() override;

  DefaultBrowserBubbleDialogManager(const DefaultBrowserBubbleDialogManager&) =
      delete;
  DefaultBrowserBubbleDialogManager& operator=(
      const DefaultBrowserBubbleDialogManager&) = delete;

  // Shows the default browser dialog on all eligible browser windows.
  // `controller` is the default browser controller to use for the dialog.
  // `can_pin_to_taskbar` indicates if the pin-to-taskbar option should be
  // enabled.
  void Show(
      std::unique_ptr<default_browser::DefaultBrowserController> controller,
      bool can_pin_to_taskbar);

  // Closes all currently open default browser dialogs.
  void CloseAll();

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
