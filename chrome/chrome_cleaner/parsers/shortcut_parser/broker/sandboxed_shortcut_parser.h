// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PARSERS_SHORTCUT_PARSER_BROKER_SANDBOXED_SHORTCUT_PARSER_H_
#define CHROME_CHROME_CLEANER_PARSERS_SHORTCUT_PARSER_BROKER_SANDBOXED_SHORTCUT_PARSER_H_

#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/parser_interface.mojom.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "chrome/chrome_cleaner/parsers/parser_utils/parse_tasks_remaining_counter.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/shortcut_parser_api.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chrome_cleaner {

class SandboxedShortcutParser : public ShortcutParserAPI {
 public:
  SandboxedShortcutParser(MojoTaskRunner* mojo_task_runner,
                          mojo::Remote<mojom::Parser>* parser);

  // ShortcutParserAPI
  void FindAndParseChromeShortcutsInFoldersAsync(
      const std::vector<base::FilePath>& folders,
      const FilePathSet& chrome_exe_locations,
      ShortcutsParsingDoneCallback callback) override;

  void ParseShortcut(base::win::ScopedHandle shortcut_handle,
                     mojom::Parser::ParseShortcutCallback callback) override;

 private:
  void FindAndParseChromeShortcutsInFolders(
      const std::vector<base::FilePath>& paths_to_explore,
      const FilePathSet& chrome_exe_locations,
      ShortcutsParsingDoneCallback callback);

  void OnShortcutsParsingDone(
      const FilePathSet& chrome_exe_locations,
      const base::FilePath& lnk_path,
      scoped_refptr<ParseTasksRemainingCounter> counter,
      std::vector<ShortcutInformation>* found_shortcuts,
      mojom::LnkParsingResult parsing_result,
      const base::Optional<base::string16>& optional_file_path,
      const base::Optional<base::string16>& optional_command_line_arguments,
      const base::Optional<base::string16>& optional_icon_location);

  base::Lock lock_;
  MojoTaskRunner* mojo_task_runner_;
  mojo::Remote<mojom::Parser>* parser_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PARSERS_SHORTCUT_PARSER_BROKER_SANDBOXED_SHORTCUT_PARSER_H_
