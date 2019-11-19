// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PARSERS_SHORTCUT_PARSER_BROKER_FAKE_SHORTCUT_PARSER_H_
#define CHROME_CHROME_CLEANER_PARSERS_SHORTCUT_PARSER_BROKER_FAKE_SHORTCUT_PARSER_H_

#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/mojom/parser_interface.mojom.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/shortcut_parser_api.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_cleaner {

class FakeShortcutParser : public ShortcutParserAPI {
 public:
  FakeShortcutParser();
  ~FakeShortcutParser() override;

  void SetShortcutsToReturn(const std::vector<ShortcutInformation>& shortcuts);

  // ShortcutParserAPI
  void ParseShortcut(base::win::ScopedHandle shortcut_handle,
                     mojom::Parser::ParseShortcutCallback callback) override;
  void FindAndParseChromeShortcutsInFoldersAsync(
      const std::vector<base::FilePath>& unused_paths,
      const FilePathSet& unused_chrome_exe_locations,
      ShortcutsParsingDoneCallback callback) override;

 private:
  std::vector<ShortcutInformation> shortcuts_to_return_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PARSERS_SHORTCUT_PARSER_BROKER_FAKE_SHORTCUT_PARSER_H_
