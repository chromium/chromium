// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"

#include "base/logging.h"
#include "build/build_config.h"

namespace web_app {

// This block defines stub implementations of OS specific methods for
// FileHandling. Currently, Windows and Desktop Linux (but not Chrome OS) have
// their own implementations.
//
// Note: Because OS_LINUX includes OS_CHROMEOS be sure to use the stub on
// OS_CHROMEOS.
#if !defined(OS_WIN) && !(defined(OS_LINUX) && !defined(OS_CHROMEOS))
bool OsSupportsWebAppFileHandling() {
  return false;
}

void RegisterFileHandlersForWebApp(const AppId& app_id,
                                   const std::string& app_name,
                                   Profile* profile,
                                   const std::set<std::string>& file_extensions,
                                   const std::set<std::string>& mime_types) {
  DCHECK(OsSupportsWebAppFileHandling());
  // Stub function for OS's that don't support Web App file handling yet.
}

void UnregisterFileHandlersForWebApp(const AppId& app_id, Profile* profile) {
  DCHECK(OsSupportsWebAppFileHandling());
  // Stub function for OS's that don't support Web App file handling yet.
}
#endif

}  // namespace web_app
