// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/sandboxed_shortcut_parser.h"

#include <stdio.h>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/task/post_task.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/mojom/parser_interface.mojom.h"
#include "chrome/chrome_cleaner/parsers/parser_utils/parse_tasks_remaining_counter.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace chrome_cleaner {

SandboxedShortcutParser::SandboxedShortcutParser(
    MojoTaskRunner* mojo_task_runner,
    mojo::Remote<mojom::Parser>* parser)
    : mojo_task_runner_(mojo_task_runner), parser_(parser) {}

void SandboxedShortcutParser::ParseShortcut(
    base::win::ScopedHandle shortcut_handle,
    mojom::Parser::ParseShortcutCallback callback) {
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](mojo::Remote<mojom::Parser>* parser, mojo::ScopedHandle handle,
             mojom::Parser::ParseShortcutCallback callback) {
            (*parser)->ParseShortcut(std::move(handle), std::move(callback));
          },
          parser_, mojo::WrapPlatformFile(shortcut_handle.Take()),
          std::move(callback)));
}

void SandboxedShortcutParser::FindAndParseChromeShortcutsInFoldersAsync(
    const std::vector<base::FilePath>& folders,
    const FilePathSet& chrome_exe_locations,
    ShortcutsParsingDoneCallback callback) {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::WithBaseSyncPrimitives()},
      base::BindOnce(
          &SandboxedShortcutParser::FindAndParseChromeShortcutsInFolders,
          base::Unretained(this), folders, chrome_exe_locations,
          std::move(callback)));
}

void SandboxedShortcutParser::FindAndParseChromeShortcutsInFolders(
    const std::vector<base::FilePath>& folders,
    const FilePathSet& chrome_exe_locations,
    ShortcutsParsingDoneCallback callback) {
  if (folders.empty()) {
    std::move(callback).Run({});
    return;
  }

  std::vector<base::FilePath> enumerated_shortcuts;
  for (const auto& folder : folders) {
    if (base::DirectoryExists(folder)) {
      base::FileEnumerator lnk_enumerator(folder, /*recursive=*/false,
                                          base::FileEnumerator::FileType::FILES,
                                          L"*.lnk");
      base::FilePath lnk_path = lnk_enumerator.Next();
      while (!lnk_path.empty()) {
        enumerated_shortcuts.push_back(lnk_path);
        lnk_path = lnk_enumerator.Next();
      }
    }
  }

  if (enumerated_shortcuts.size() == 0) {
    std::move(callback).Run({});
    return;
  }

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  scoped_refptr<ParseTasksRemainingCounter> counter =
      base::MakeRefCounted<ParseTasksRemainingCounter>(
          enumerated_shortcuts.size(), &event);

  std::vector<ShortcutInformation> parsed_shortcuts;
  for (const auto& lnk_path : enumerated_shortcuts) {
    base::File lnk_file(
        lnk_path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
    ParseShortcut(
        base::win::ScopedHandle(lnk_file.TakePlatformFile()),
        base::BindOnce(&SandboxedShortcutParser::OnShortcutsParsingDone,
                       base::Unretained(this), chrome_exe_locations, lnk_path,
                       counter, &parsed_shortcuts));
  }
  event.Wait();
  std::move(callback).Run(parsed_shortcuts);
}

void SandboxedShortcutParser::OnShortcutsParsingDone(
    const FilePathSet& chrome_exe_locations,
    const base::FilePath& lnk_path,
    scoped_refptr<ParseTasksRemainingCounter> counter,
    std::vector<ShortcutInformation>* found_shortcuts,
    mojom::LnkParsingResult parsing_result,
    const base::Optional<base::string16>& optional_file_path,
    const base::Optional<base::string16>& optional_command_line_arguments,
    const base::Optional<base::string16>& optional_icon_location) {
  ShortcutInformation parsed_shortcut;
  parsed_shortcut.lnk_path = lnk_path;
  if (parsing_result == mojom::LnkParsingResult::SUCCESS) {
    if (optional_file_path.has_value())
      parsed_shortcut.target_path = optional_file_path.value();

    if (optional_command_line_arguments.has_value()) {
      parsed_shortcut.command_line_arguments =
          optional_command_line_arguments.value();
    }

    if (optional_icon_location.has_value())
      parsed_shortcut.icon_location = optional_icon_location.value();

    const base::string16 kChromeLnkName = L"Google Chrome.lnk";
    if (chrome_exe_locations.Contains(
            base::FilePath(parsed_shortcut.icon_location)) ||
        lnk_path.BaseName().value() == kChromeLnkName) {
      base::AutoLock lock(lock_);
      found_shortcuts->push_back(parsed_shortcut);
    }
  }

  {
    base::AutoLock lock(lock_);
    counter->Decrement();
  }
}

}  // namespace chrome_cleaner
