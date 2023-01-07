// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMANDER_WINDOW_COMMAND_SOURCE_H_
#define CHROME_BROWSER_UI_COMMANDER_WINDOW_COMMAND_SOURCE_H_

#include "chrome/browser/ui/commander/command_source.h"

namespace commander {

// Command source for window-related commands.
class WindowCommandSource : public CommandSource {
 public:
  WindowCommandSource();
  ~WindowCommandSource() override;

  WindowCommandSource(const WindowCommandSource& other) = delete;
  WindowCommandSource& operator=(const WindowCommandSource& other) = delete;

  // Command source overrides
  CommandSource::CommandResults GetCommands(const std::u16string& input,
                                            Browser* browser) const override;
};
}  // namespace commander

#endif  // CHROME_BROWSER_UI_COMMANDER_WINDOW_COMMAND_SOURCE_H_
