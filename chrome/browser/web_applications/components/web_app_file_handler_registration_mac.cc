// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"

namespace web_app {
bool ShouldRegisterFileHandlersWithOs() {
  return true;
}

void RegisterFileHandlersWithOs(const AppId& app_id,
                                const std::string& app_name,
                                Profile* profile,
                                const apps::FileHandlers& file_handlers) {
  // On MacOS, file associations are managed through app shims in the
  // Applications directory. File handler registration is handled via shortcuts
  // creation.
  NOTREACHED();
}

void UnregisterFileHandlersWithOs(const AppId& app_id,
                                  Profile* profile,
                                  std::unique_ptr<ShortcutInfo> info,
                                  base::OnceCallback<void()> callback) {
  // On MacOS, file associations are managed through app shims in the
  // Applications directory. File handler unregistration is handled via
  // shortcuts deletion on MacOS.
  NOTREACHED();
  std::move(callback).Run();
}

}  // namespace web_app
