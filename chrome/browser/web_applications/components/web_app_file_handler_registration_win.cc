// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration_win.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/profiles/profile.h"

namespace web_app {

const base::FilePath::StringPieceType kLastBrowserFile(
    FILE_PATH_LITERAL("Last Browser"));

bool OsSupportsWebAppFileHandling() {
  return true;
}

void RegisterFileHandlersForWebApp(const AppId& app_id,
                                   const std::string& app_name,
                                   Profile* profile,
                                   const std::set<std::string>& file_extensions,
                                   const std::set<std::string>& mime_types) {
  // TODO(davidbienvenu): Setup shim app and windows registry for this |app_id|.
}

void UnregisterFileHandlersForWebApp(const AppId& app_id, Profile* profile) {
  // TODO(davidbienvenu): Cleanup windows registry entries for this |app_id|.
}

void UpdateChromeExePath(const base::FilePath& user_data_dir) {
  DCHECK(!user_data_dir.empty());
  base::FilePath chrome_exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &chrome_exe_path))
    return;
  const base::FilePath::StringType& chrome_exe_path_str =
      chrome_exe_path.value();
  DCHECK(!chrome_exe_path_str.empty());
  base::WriteFile(
      user_data_dir.Append(kLastBrowserFile),
      reinterpret_cast<const char*>(chrome_exe_path_str.data()),
      chrome_exe_path_str.size() * sizeof(base::FilePath::CharType));
}

}  // namespace web_app
