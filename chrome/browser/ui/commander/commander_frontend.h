// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMANDER_COMMANDER_FRONTEND_H_
#define CHROME_BROWSER_UI_COMMANDER_COMMANDER_FRONTEND_H_

#include <memory>

class Browser;

namespace commander {

class CommanderBackend;

// Abstract interface for the commander UI.
class CommanderFrontend {
 public:
  CommanderFrontend() = default;
  virtual ~CommanderFrontend() = default;

  // If the UI is currently showing for |browser|, hides it.
  // If the UI is currently showing for a different browser,
  // hides it, then shows it for |browser|.
  // If the UI is not showing, shows it for |browser|.
  virtual void ToggleForBrowser(Browser* browser) = 0;
  // Show the UI, anchored to |browser|'s window.
  virtual void Show(Browser* browser) = 0;
  // Hide the UI, if showing.
  virtual void Hide() = 0;

  static std::unique_ptr<CommanderFrontend> Create(CommanderBackend* backend);

  // Disallow copy and assign
  CommanderFrontend(const CommanderFrontend& other) = delete;
  CommanderFrontend& operator=(const CommanderFrontend& other) = delete;
};

}  // namespace commander

#endif  // CHROME_BROWSER_UI_COMMANDER_COMMANDER_FRONTEND_H_
