// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/configure_app_container_sandbox.h"

#include <windows.h>

#include <string_view>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/win/security_util.h"
#include "base/win/sid.h"
#include "base/win/win_util.h"

namespace installer {

bool ConfigureAppContainerSandbox(base::span<const base::FilePath*> paths) {
  static constexpr std::wstring_view kChromeInstallFilesCapabilitySid(
      L"S-1-15-3-1024-3424233489-972189580-2057154623-747635277-1604371224-"
      L"316187997-3786583170-1043257646");
  static constexpr std::wstring_view kLpacChromeInstallFilesCapabilitySid(
      L"S-1-15-3-1024-2302894289-466761758-1166120688-1039016420-2430351297-"
      L"4240214049-4028510897-3317428798");

  auto sids = base::win::Sid::FromSddlStringVector(
      {std::wstring(kChromeInstallFilesCapabilitySid),
       std::wstring(kLpacChromeInstallFilesCapabilitySid)});

  if (!sids) {
    return false;
  }

  bool success = true;

  for (const base::FilePath* path : paths) {
    success = base::win::GrantAccessToPath(
                  *path, *sids, FILE_GENERIC_READ | FILE_GENERIC_EXECUTE,
                  CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE) &&
              success;
  }

  return success;
}

}  // namespace installer
