// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/elevated_tracing_service/service_integration.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"

namespace elevated_tracing_service {

std::wstring GetStorageDirBasename() {
  return base::StrCat(
      {std::wstring_view(install_static::kProductPathName,
                         install_static::kProductPathNameLength),
       install_static::InstallDetails::Get().install_suffix(), L"Tracing"});
}

}  // namespace elevated_tracing_service
