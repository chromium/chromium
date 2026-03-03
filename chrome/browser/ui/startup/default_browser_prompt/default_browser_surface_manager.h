// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_SURFACE_MANAGER_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_SURFACE_MANAGER_H_

#include <memory>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

class BrowserWindowInterface;

namespace default_browser {
class DefaultBrowserController;
enum class DefaultBrowserEntrypointType;
}  // namespace default_browser

// Abstract base class for managing the display of default browser prompts.
// This class handles the logic for observing browser window creation and
// destruction, ensuring prompts are shown on all appropriate windows.
// Subclasses must implement the specific UI presentation logic.
class DefaultBrowserSurfaceManager : public BrowserCollectionObserver {
 public:
  DefaultBrowserSurfaceManager();
  ~DefaultBrowserSurfaceManager() override;

  DefaultBrowserSurfaceManager(const DefaultBrowserSurfaceManager&) = delete;
  DefaultBrowserSurfaceManager& operator=(const DefaultBrowserSurfaceManager&) =
      delete;

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) final;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

  // Shows the default browser prompt. `can_pin_to_taskbar` controls whether
  // the pin option is available.
  virtual void Show(bool can_pin_to_taskbar);

  // Closes all open prompts managed by this surface manager.
  virtual void CloseAll();

  // Returns the entry point type associated with this surface.
  virtual default_browser::DefaultBrowserEntrypointType GetEntrypointType()
      const = 0;

  bool can_pin_to_taskbar() const { return can_pin_to_taskbar_; }

  // Methods for derived classes to handle user interactions via the controller.
  void HandleAccept();
  void HandleDismiss();
  void HandleIgnore();

 protected:
  // Helper function to determine if a browser window is suitable for showing a
  // prompt. Excludes incognito, guest, and non-normal browser windows.
  bool IsBrowserValidForShowing(BrowserWindowInterface* browser);

 private:
  // Abstract methods to be implemented by subclasses to handle UI operations.
  //
  // Shows the specific prompt UI for the given browser window.
  virtual void ShowForBrowser(BrowserWindowInterface* browser) = 0;

  // Closes the specific prompt UI associated with the given browser window.
  virtual void CloseForBrowser(BrowserWindowInterface* browser) = 0;

  // Closes all prompt instances managed by the subclass.
  virtual void CloseAllPromptInstances() = 0;

  // Flag indicating if the taskbar pinning option should be available.
  bool can_pin_to_taskbar_ = false;

  // The controller instance managing the default browser flow.
  std::unique_ptr<default_browser::DefaultBrowserController> controller_;

  // Scoped observation for tracking browser window creation and destruction.
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_SURFACE_MANAGER_H_
