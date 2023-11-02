// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMANDER_OPEN_URL_COMMAND_SOURCE_H_
#define CHROME_BROWSER_UI_COMMANDER_OPEN_URL_COMMAND_SOURCE_H_

#include "chrome/browser/ui/commander/command_source.h"
#include "url/gurl.h"

namespace commander {

// A command source for basic commands that open a given URL in a new tab.
class OpenURLCommandSource : public CommandSource {
 public:
  OpenURLCommandSource();
  ~OpenURLCommandSource() override;

  // Disallow copy and assign.
  OpenURLCommandSource(const OpenURLCommandSource& other) = delete;
  OpenURLCommandSource& operator=(const OpenURLCommandSource& other) = delete;

  // CommandSource overrides
  CommandSource::CommandResults GetCommands(const std::u16string& input,
                                            Browser* browser) const override;

 private:
  const std::vector<std::pair<std::u16string, GURL>> title_url_map_;
};

}  // namespace commander

#endif  // CHROME_BROWSER_UI_COMMANDER_OPEN_URL_COMMAND_SOURCE_H_
