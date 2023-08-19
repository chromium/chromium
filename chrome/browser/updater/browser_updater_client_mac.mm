// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client.h"

#include <string>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/strings/strcat.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/version_info.h"

namespace {

std::string GetTag() {
  std::string contents;
  base::ReadFileToString(
      base::apple::OuterBundlePath().Append(".want_full_installer"), &contents);
  return base::StrCat(
      {chrome::GetChannelName(chrome::WithExtendedStable(true)),
       contents == version_info::GetVersionNumber() ? "-full" : ""});
}

}  // namespace

std::string BrowserUpdaterClient::GetAppId() {
  return base::apple::BaseBundleID();
}

updater::RegistrationRequest BrowserUpdaterClient::GetRegistrationRequest() {
  updater::RegistrationRequest req;
  req.app_id = GetAppId();
  google_brand::GetBrand(&req.brand_code);
  req.version = base::Version(version_info::GetVersionNumber());
  req.ap = GetTag();
  req.existence_checker_path = base::apple::OuterBundlePath();
  return req;
}
