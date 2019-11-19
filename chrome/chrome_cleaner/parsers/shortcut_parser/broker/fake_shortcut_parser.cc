// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/fake_shortcut_parser.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/mojom/parser_interface.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace chrome_cleaner {

FakeShortcutParser::FakeShortcutParser() = default;
FakeShortcutParser::~FakeShortcutParser() = default;

void FakeShortcutParser::SetShortcutsToReturn(
    const std::vector<ShortcutInformation>& shortcuts) {
  shortcuts_to_return_ = std::vector<ShortcutInformation>(shortcuts);
}

void FakeShortcutParser::ParseShortcut(
    base::win::ScopedHandle shortcut_handle,
    mojom::Parser::ParseShortcutCallback callback) {}

void FakeShortcutParser::FindAndParseChromeShortcutsInFoldersAsync(
    const std::vector<base::FilePath>& unused_paths,
    const FilePathSet& unused_chrome_exe_locations,
    ShortcutsParsingDoneCallback callback) {
  std::move(callback).Run(shortcuts_to_return_);
}

}  // namespace chrome_cleaner
