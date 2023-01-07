// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/shortcut_parser/sandboxed_lnk_parser_test_util.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/win/scoped_handle.h"
#include "base/win/shortcut.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/target/lnk_parser.h"

namespace chrome_cleaner {

base::win::ScopedHandle CreateAndOpenShortcutInTempDir(
    const std::string& lnk_name,
    const base::win::ShortcutProperties& properties,
    base::ScopedTempDir* temp_dir) {
  base::FilePath shortcut_path =
      temp_dir->GetPath().AppendASCII(lnk_name.c_str());
  if (!base::win::CreateOrUpdateShortcutLink(
          shortcut_path, properties,
          base::win::ShortcutOperation::kCreateAlways)) {
    LOG(ERROR) << "Could not create shortcut";
    return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
  }

  base::File lnk_file(shortcut_path, base::File::Flags::FLAG_OPEN |
                                         base::File::Flags::FLAG_READ);
  base::win::ScopedHandle handle(lnk_file.TakePlatformFile());
  if (!handle.IsValid())
    LOG(ERROR) << "Error opening the lnk file";
  return handle;
}

bool CheckParsedShortcut(const ParsedLnkFile& parsed_shortcut,
                         base::FilePath expected_target_path,
                         base::FilePath expected_working_dir,
                         std::wstring expected_arguments,
                         base::FilePath expected_icon_location,
                         const int32_t expected_icon_index) {
  base::FilePath parsed_target_path(parsed_shortcut.target_path);
  base::FilePath parsed_working_dir_path(parsed_shortcut.working_dir);
  return PathEqual(parsed_target_path, expected_target_path) &&
         PathEqual(parsed_working_dir_path, expected_working_dir) &&
         (parsed_shortcut.command_line_arguments == expected_arguments) &&
         PathEqual(expected_target_path, expected_icon_location) &&
         (parsed_shortcut.icon_index == expected_icon_index);
}

void OnLnkParseDone(
    ParsedLnkFile* out_parsed_shortcut,
    mojom::LnkParsingResult* out_result_code,
    base::OnceClosure callback,
    mojom::LnkParsingResult result_code,
    const absl::optional<std::wstring>& optional_target_path,
    const absl::optional<std::wstring>& optional_working_dir,
    const absl::optional<std::wstring>& optional_command_line_arguments,
    const absl::optional<std::wstring>& optional_icon_location,
    int32_t icon_index) {
  *out_result_code = result_code;
  if (optional_target_path.has_value())
    out_parsed_shortcut->target_path = optional_target_path.value();

  if (optional_working_dir.has_value())
    out_parsed_shortcut->working_dir = optional_working_dir.value();

  if (optional_command_line_arguments.has_value()) {
    out_parsed_shortcut->command_line_arguments =
        optional_command_line_arguments.value();
  }

  if (optional_icon_location.has_value()) {
    out_parsed_shortcut->icon_location = optional_icon_location.value();
    out_parsed_shortcut->icon_index = icon_index;
  }

  std::move(callback).Run();
}

}  // namespace chrome_cleaner
