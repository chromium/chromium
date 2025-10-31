// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_ENTRY_POINT_CONTROLLER_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_ENTRY_POINT_CONTROLLER_H_

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/actions/actions.h"

namespace read_anything {

// Maintains and handles entrypoints for the ReadAnythingController.
class ReadAnythingEntryPointController {
 public:
  ReadAnythingEntryPointController(const ReadAnythingEntryPointController&) =
      delete;
  ReadAnythingEntryPointController& operator=(
      const ReadAnythingEntryPointController&) = delete;
  ~ReadAnythingEntryPointController();

  // Triggered when the omnibox chip or pinned side panel entrypoint is clicked.
  static void InvokePageAction(BrowserWindowInterface* bwi,
                               const actions::ActionInvocationContext& context);

  // Shows or hides the omnibox entry point.
  static void UpdatePageActionVisibility(bool should_show_page_action,
                                         BrowserWindowInterface* bwi);
};

}  // namespace read_anything

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_ENTRY_POINT_CONTROLLER_H_
