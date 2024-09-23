// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client.h"

#include <string>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/version_info.h"

std::string BrowserUpdaterClient::GetAppId() {
  return base::apple::BaseBundleID();
}

updater::RegistrationRequest BrowserUpdaterClient::GetRegistrationRequest() {
  base::FilePath bundle = base::apple::OuterBundlePath();
  updater::RegistrationRequest req;
  req.app_id = GetAppId();
  google_brand::GetBrand(&req.brand_code);
  req.version = base::Version(version_info::GetVersionNumber());
  req.version_path = bundle.AppendASCII("Contents").AppendASCII("Info.plist");
  req.version_key = "KSVersion";
  req.ap = chrome::GetChannelName(chrome::WithExtendedStable(true));
  req.existence_checker_path = bundle;
  return req;
}

bool BrowserUpdaterClient::AppMatches(
    const updater::UpdateService::AppState& app) {
  return base::EqualsCaseInsensitiveASCII(app.app_id, GetAppId()) &&
         app.ecp == base::apple::OuterBundlePath();
}
