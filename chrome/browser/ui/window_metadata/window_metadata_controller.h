// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WINDOW_METADATA_WINDOW_METADATA_CONTROLLER_H_
#define CHROME_BROWSER_UI_WINDOW_METADATA_WINDOW_METADATA_CONTROLLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/image/image.h"

class Browser;

namespace content {
class WebContents;
}  // namespace content

// Manages window title computation and favicon retrieval for a browser window.
// This is a UI-centric controller that depends on the active tab's state and
// window parameters to compute the appropriate title and icon.
//
// Registered via ScopedUnownedUserData on BrowserWindowInterface.
class WindowMetadataController {
 public:
  DECLARE_USER_DATA(WindowMetadataController);

  static WindowMetadataController* From(BrowserWindowInterface* browser);
  static const WindowMetadataController* From(
      const BrowserWindowInterface* browser);

  WindowMetadataController(BrowserWindowInterface& browser,
                           const std::string& initial_user_title);
  ~WindowMetadataController();

  WindowMetadataController(const WindowMetadataController&) = delete;
  WindowMetadataController& operator=(const WindowMetadataController&) = delete;

  // Gets the favicon of the page in the selected tab.
  gfx::Image GetCurrentPageIcon() const;

  // Gets the title of the window based on the selected tab's title.
  // Disables additional formatting when |include_app_name| is false or if the
  // window is an app window.
  std::u16string GetWindowTitleForCurrentTab(bool include_app_name) const;

  // Gets the window title of the given tab.
  std::u16string GetWindowTitleForTab(const tabs::TabHandle& tab) const;

  // Gets the formatted title for the given tab.
  std::u16string GetTitleForTab(const tabs::TabHandle& tab) const;

  // Gets the window title for the current tab, to display in a menu. If the
  // title is too long to fit in the required space, the tab title will be
  // elided. The result title might still be a larger width than specified, as
  // at least a few characters of the title are always shown.
  std::u16string GetWindowTitleForMaxWidth(int max_width) const;

  // Gets the window title from the provided WebContents.
  // Disables additional formatting when |include_app_name| is false or if the
  // window is an app window.
  std::u16string GetWindowTitleFromWebContents(
      bool include_app_name,
      content::WebContents* contents) const;

  // Prepares a title string for display (removes embedded newlines, etc).
  static std::u16string FormatTitleForDisplay(std::u16string title);

  // Sets the browser's user title. Setting it to an empty string clears it.
  // Updates the title bar and syncs with SessionService.
  void SetWindowUserTitle(const std::string& user_title);

  // Returns the user-defined window title.
  const std::string& user_title() const { return user_title_; }

 private:
  // TODO(crbug.com/496674143): Migrate to use BrowserWindowInterface directly
  // once the needed methods are available on the interface.
  raw_ptr<Browser> browser_;

  // User-defined window title. Empty if not set.
  std::string user_title_;

  ui::ScopedUnownedUserData<WindowMetadataController> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_WINDOW_METADATA_WINDOW_METADATA_CONTROLLER_H_
