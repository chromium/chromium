// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client.h"

#include <string>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/strings/sys_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/install_static/install_details.h"
#include "chrome/updater/registration_data.h"
#include "components/version_info/version_info.h"

namespace updater {

std::string BrowserUpdaterClient::GetAppId() {
  return base::SysWideToUTF8(
      base::ToLowerASCII(install_static::InstallDetails::Get().app_guid()));
}

base::FilePath BrowserUpdaterClient::GetExpectedEcp() {
  return {};
}

RegistrationRequest BrowserUpdaterClient::GetRegistrationRequest() {
  RegistrationRequest req;
  req.app_id = GetAppId();
  google_brand::GetBrand(&req.brand_code);
  req.version = version_info::GetVersionNumber();
  req.ap =
      base::SysWideToUTF8(install_static::InstallDetails::Get().update_ap());
  return req;
}

bool BrowserUpdaterClient::AppMatches(const UpdateService::AppState& app) {
  return base::EqualsCaseInsensitiveASCII(app.app_id, GetAppId());
}

}  // namespace updater
