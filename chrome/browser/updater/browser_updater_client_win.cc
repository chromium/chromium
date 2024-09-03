// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client.h"

#include <string>

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/install_static/install_details.h"
#include "components/version_info/version_info.h"

std::string BrowserUpdaterClient::GetAppId() {
  return base::SysWideToUTF8(
      std::wstring(install_static::InstallDetails::Get().app_guid()));
}

updater::RegistrationRequest BrowserUpdaterClient::GetRegistrationRequest() {
  updater::RegistrationRequest req;
  req.app_id = GetAppId();
  google_brand::GetBrand(&req.brand_code);
  req.version = base::Version(version_info::GetVersionNumber());
  req.ap =
      base::SysWideToUTF8(install_static::InstallDetails::Get().update_ap());
  return req;
}

bool BrowserUpdaterClient::AppMatches(
    const updater::UpdateService::AppState& app) {
  return base::EqualsCaseInsensitiveASCII(app.app_id, GetAppId());
}
