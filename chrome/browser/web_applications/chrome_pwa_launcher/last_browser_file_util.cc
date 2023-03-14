// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chrome_pwa_launcher/last_browser_file_util.h"

#include <string>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"

namespace web_app {

const base::FilePath::StringPieceType kLastBrowserFilename =
    FILE_PATH_LITERAL("Last Browser");

base::FilePath ReadChromePathFromLastBrowserFile(
    const base::FilePath& last_browser_file) {
  std::string last_browser_file_data;
  if (!base::ReadFileToStringWithMaxSize(
          last_browser_file, &last_browser_file_data,
          MAX_PATH * sizeof(base::FilePath::CharType))) {
    return base::FilePath();
  }

  base::FilePath::StringType chrome_path(
      reinterpret_cast<const base::FilePath::CharType*>(
          last_browser_file_data.data()),
      last_browser_file_data.size() / sizeof(base::FilePath::CharType));
  const base::FilePath::StringPieceType chrome_path_trimmed =
      base::TrimString(chrome_path, FILE_PATH_LITERAL(" \n"), base::TRIM_ALL);
  return base::FilePath(chrome_path_trimmed);
}

void WriteChromePathToLastBrowserFile(const base::FilePath& user_data_dir) {
  DCHECK(!user_data_dir.empty());
  base::FilePath chrome_path;
  if (!base::PathService::Get(base::FILE_EXE, &chrome_path))
    return;

  const base::FilePath::StringType& chrome_path_str = chrome_path.value();
  DCHECK(!chrome_path_str.empty());
  base::WriteFile(user_data_dir.Append(kLastBrowserFilename),
                  base::as_bytes(base::make_span(chrome_path_str)));
}

base::FilePath GetLastBrowserFileFromWebAppDir(
    const base::FilePath& web_app_dir) {
  return web_app_dir.DirName().DirName().DirName().Append(kLastBrowserFilename);
}

}  // namespace web_app
