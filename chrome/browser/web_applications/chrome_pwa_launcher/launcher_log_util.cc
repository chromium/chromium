// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chrome_pwa_launcher/launcher_log_util.h"

#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"

namespace web_app {

const wchar_t kPwaLauncherResult[] = L"PWALauncherResult";

base::win::RegKey GetLauncherLogRegistryKey(REGSAM access) {
  return base::win::RegKey(HKEY_CURRENT_USER,
                           install_static::GetRegistryPath().c_str(), access);
}

}  // namespace web_app
