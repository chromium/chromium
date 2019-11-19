// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PARSERS_SHORTCUT_PARSER_BROKER_SHORTCUT_PARSER_API_H_
#define CHROME_CHROME_CLEANER_PARSERS_SHORTCUT_PARSER_BROKER_SHORTCUT_PARSER_API_H_

#include <set>
#include <vector>

#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/mojom/parser_interface.mojom.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"

namespace chrome_cleaner {

// Holds information about parsed shortcuts that is useful for logging purposes.
// This structure is intended to be used from the broker side while the
// |ParsedLnkFile| structure is intended to be used from the Target side.
struct ShortcutInformation {
  ShortcutInformation();
  ShortcutInformation(const ShortcutInformation& other);
  ~ShortcutInformation();

  base::FilePath lnk_path;
  base::string16 target_path;
  base::string16 command_line_arguments;
  base::string16 icon_location;
};

typedef base::OnceCallback<void(std::vector<ShortcutInformation>)>
    ShortcutsParsingDoneCallback;

class ShortcutParserAPI {
 public:
  virtual ~ShortcutParserAPI() = default;
  virtual void ParseShortcut(base::win::ScopedHandle shortcut_handle,
                             mojom::Parser::ParseShortcutCallback callback) = 0;

  // Search through the paths specified in |folders| and reports the lnk files
  // whose Icon Path is listed in |chrome_exe_locations| or whose name is
  // Google Chrome.lnk.
  virtual void FindAndParseChromeShortcutsInFoldersAsync(
      const std::vector<base::FilePath>& folders,
      const FilePathSet& chrome_exe_locations,
      ShortcutsParsingDoneCallback callback) = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PARSERS_SHORTCUT_PARSER_BROKER_SHORTCUT_PARSER_API_H_
