// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMANDER_BOOKMARK_COMMAND_SOURCE_H_
#define CHROME_BROWSER_UI_COMMANDER_BOOKMARK_COMMAND_SOURCE_H_

#include "chrome/browser/ui/commander/command_source.h"

namespace commander {

// Provides an "Open Bookmark..." composite command which lets the user
// search for a bookmark to open. If the user has typed a minimum threshold
// of characters, this will also return matching individual bookmark commands
// directly.
class BookmarkCommandSource : public CommandSource {
 public:
  BookmarkCommandSource();
  ~BookmarkCommandSource() override;

  BookmarkCommandSource(const BookmarkCommandSource& other) = delete;
  BookmarkCommandSource& operator=(const BookmarkCommandSource& other) = delete;

  // Command source overrides
  CommandSource::CommandResults GetCommands(const std::u16string& input,
                                            Browser* browser) const override;
};

}  // namespace commander

#endif  // CHROME_BROWSER_UI_COMMANDER_BOOKMARK_COMMAND_SOURCE_H_
