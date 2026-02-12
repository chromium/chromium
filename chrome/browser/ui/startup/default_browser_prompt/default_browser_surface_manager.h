// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_SURFACE_MANAGER_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_SURFACE_MANAGER_H_

#include <memory>

#include "chrome/browser/default_browser/default_browser_controller.h"

// Interface that allows DefaultBrowserPromptManager to show different types of
// UI based on Finch configuration.
class DefaultBrowserSurfaceManager {
 public:
  DefaultBrowserSurfaceManager() = default;
  virtual ~DefaultBrowserSurfaceManager() = default;

  DefaultBrowserSurfaceManager(const DefaultBrowserSurfaceManager&) = delete;
  DefaultBrowserSurfaceManager& operator=(const DefaultBrowserSurfaceManager&) =
      delete;

  // Shows the default browser prompt. `controller` handles the execution logic
  // and `can_pin_to_taskbar` controls whether the pin option is available.
  virtual void Show(
      std::unique_ptr<default_browser::DefaultBrowserController> controller,
      bool can_pin_to_taskbar) = 0;

  // Closes all open prompts managed by this surface manager.
  virtual void CloseAll() = 0;

  // Returns the entry point type associated with this surface.
  virtual default_browser::DefaultBrowserEntrypointType GetEntrypointType()
      const = 0;
};

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_SURFACE_MANAGER_H_
