// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMANDER_APPS_COMMAND_SOURCE_H_
#define CHROME_BROWSER_UI_COMMANDER_APPS_COMMAND_SOURCE_H_

#include "chrome/browser/ui/commander/command_source.h"

namespace commander {

// A command source for interacting with GSuite.
class AppsCommandSource : public CommandSource {
 public:
  AppsCommandSource();
  ~AppsCommandSource() override;

  // Disallow copy and assign.
  AppsCommandSource(const AppsCommandSource& other) = delete;
  AppsCommandSource& operator=(const AppsCommandSource& other) = delete;

  // CommandSource overrides
  CommandSource::CommandResults GetCommands(const base::string16& input,
                                            Browser* browser) const override;
};

}  // namespace commander

#endif  // CHROME_BROWSER_UI_COMMANDER_APPS_COMMAND_SOURCE_H_
