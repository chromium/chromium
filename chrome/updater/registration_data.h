// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_REGISTRATION_DATA_H_
#define CHROME_UPDATER_REGISTRATION_DATA_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/version.h"
#include "chrome/updater/constants.h"

namespace updater {

inline constexpr int kRegistrationSuccess = 0;
inline constexpr int kRegistrationError = 1;

struct RegistrationRequest {
  RegistrationRequest();
  RegistrationRequest(const RegistrationRequest&);
  RegistrationRequest& operator=(const RegistrationRequest& other) = default;
  ~RegistrationRequest();

  // Application ID of the app.
  std::string app_id;

  // The brand code, a four character code attributing the app’s
  // presence to a marketing campaign or similar effort. May be the empty
  // string.
  std::string brand_code;

  // A file path. Currently applicable to Mac only: if a valid plist file
  // exists at this path, the string value of key "KSBrandID" will override
  // the `brand_code` above.
  base::FilePath brand_path;

  // The ap value (e.g. from a tagged metainstaller). May be the empty string.
  // This typically indicates channel, though it can carry additional data as
  // well.
  std::string ap;

  // A file path and key. Currently applicable to Mac only: if a valid plist
  // file exists at `ap_path` and `ap_key` is non-empty, the string value of
  // key `ap_key` will override the `ap` above.
  base::FilePath ap_path;
  std::string ap_key;

  // The version of the app already installed. 0.0.0.0 if the app is not
  // already installed.
  base::Version version;

  // A file path and key. Currently applicable to Mac only: if a valid plist
  // file exists at `version_path` and `version_key` is non-empty, the string
  // value of key `version_key` will override the `version` above.
  base::FilePath version_path;
  std::string version_key;

  // A file path. A file exists at this path if and only if the app is
  // still installed. This is used (on Mac, for example) to detect
  // whether an app has been uninstalled via deletion. May be the empty
  // string; if so, the app is assumed to be installed unconditionally.
  base::FilePath existence_checker_path;

  // Date-last-active. The value is the number of days since Jan 1, 2007.
  std::optional<int> dla;

  // Date-last-rollcall. The value is the number of days since Jan 1, 2007.
  std::optional<int> dlrc;

  // Date of install. The value is the number of days since Jan 1, 2007.
  std::optional<int> install_date;

  // Opaque cohort string meaningful to the server.
  std::string cohort;

  // Human-readable interpretation of the cohort.
  std::string cohort_name;

  // Server may use to move the app to a new cohort.
  std::string cohort_hint;
};

}  // namespace updater

#endif  // CHROME_UPDATER_REGISTRATION_DATA_H_
