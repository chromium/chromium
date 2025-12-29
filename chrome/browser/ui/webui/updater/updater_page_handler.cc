// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/updater/updater_page_handler.h"

#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"

namespace {

// Returns a vector of the per-system and per-current-user updater installation
// directories. Directories are not assumed to exist.
std::vector<base::FilePath> GetUpdaterDirectories() {
  std::vector<base::FilePath> paths;
  for (updater::UpdaterScope scope :
       {updater::UpdaterScope::kSystem, updater::UpdaterScope::kUser}) {
    std::optional<base::FilePath> install_path =
        updater::GetInstallDirectory(scope);
    if (install_path) {
      paths.push_back(*std::move(install_path));
    }
  }
  return paths;
}

// Reads an updater event log file returning the vector of newline-delimited
// messages.
std::vector<std::string> ReadUpdaterEvents(const base::FilePath& log_path) {
  if (!base::PathExists(log_path)) {
    return {};
  }
  std::string contents;
  if (!base::ReadFileToString(log_path, &contents)) {
    DPLOG(WARNING) << "Failed to read updater history log file: " << log_path;
    return {};
  }
  return base::SplitString(contents, "\n", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

}  // namespace

UpdaterPageHandler::UpdaterPageHandler(
    mojo::PendingReceiver<updater_ui::mojom::PageHandler> receiver,
    mojo::PendingRemote<updater_ui::mojom::Page> page)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {}

UpdaterPageHandler::~UpdaterPageHandler() = default;

void UpdaterPageHandler::GetAllUpdaterEvents(
    GetAllUpdaterEventsCallback callback) {
  std::vector<std::string> all_messages;
  for (const base::FilePath& directory : GetUpdaterDirectories()) {
    for (const std::string_view filename :
         {"updater_history.jsonl", "updater_history.jsonl.old"}) {
      std::vector<std::string> messages =
          ReadUpdaterEvents(directory.AppendASCII(filename));
      all_messages.reserve(all_messages.size() + messages.size());
      all_messages.insert(all_messages.end(),
                          std::make_move_iterator(messages.begin()),
                          std::make_move_iterator(messages.end()));
    }
  }
  std::move(callback).Run(all_messages);
}
