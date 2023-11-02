// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMANDER_COMMANDER_H_
#define CHROME_BROWSER_UI_COMMANDER_COMMANDER_H_

#include <memory>

#include "base/no_destructor.h"

class Browser;

namespace commander {

class CommanderBackend;
class CommanderFrontend;

// Returns true if the commander UI should be made available.
bool IsEnabled();

// A singleton which initializes and owns the components of the commander
// system. This is the boundary between commander internals and the rest of the
// browser.
class Commander {
 public:
  static Commander* Get();

  // Initialize internal components. Must be called before any calls to
  // ToggleForBrowser().
  void Initialize();

  // Toggles the UI. There are three possible cases:
  // If the UI is not showing, it will be shown attached to `browser`'s window.
  // If the UI is showing on `browser`, it will be hidden.
  // If the UI is showing on a different browser, it will be hidden, and then
  // shown attached to `browser`'s window.
  void ToggleForBrowser(Browser* browser);

  Commander(const Commander& other) = delete;
  Commander& operator=(const Commander& other) = delete;

 private:
  friend base::NoDestructor<Commander>;

  Commander();
  ~Commander();
  std::unique_ptr<CommanderFrontend> frontend_;
  std::unique_ptr<CommanderBackend> backend_;
};

}  // namespace commander

#endif  // CHROME_BROWSER_UI_COMMANDER_COMMANDER_H_
