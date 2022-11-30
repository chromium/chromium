// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PARSERS_SHORTCUT_PARSER_SANDBOXED_LNK_PARSER_TEST_UTIL_H_
#define CHROME_CHROME_CLEANER_PARSERS_SHORTCUT_PARSER_SANDBOXED_LNK_PARSER_TEST_UTIL_H_

#include <windows.h>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/win/scoped_handle.h"
#include "base/win/shortcut.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/target/lnk_parser.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chrome_cleaner {

base::win::ScopedHandle CreateAndOpenShortcutInTempDir(
    const std::string& lnk_name,
    const base::win::ShortcutProperties& properties,
    base::ScopedTempDir* temp_dir);

bool CheckParsedShortcut(const ParsedLnkFile& parsed_shortcut,
                         base::FilePath expected_target_path,
                         base::FilePath expected_working_dir,
                         std::wstring expected_arguments,
                         base::FilePath expected_icon_location,
                         const int32_t icon_index);
void OnLnkParseDone(
    ParsedLnkFile* out_parsed_shortcut,
    mojom::LnkParsingResult* out_result_code,
    base::OnceClosure callback,
    mojom::LnkParsingResult result_code,
    const absl::optional<std::wstring>& optional_target_path,
    const absl::optional<std::wstring>& optional_working_dir,
    const absl::optional<std::wstring>& optional_command_line_arguments,
    const absl::optional<std::wstring>& optional_icon_location,
    int32_t optional_icon_index);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PARSERS_SHORTCUT_PARSER_SANDBOXED_LNK_PARSER_TEST_UTIL_H_
