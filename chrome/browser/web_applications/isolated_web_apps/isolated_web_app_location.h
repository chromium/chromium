// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_LOCATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_LOCATION_H_

#include "base/files/file_path.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/origin.h"

namespace base {
class Value;
}  // namespace base

namespace web_app {

inline constexpr base::FilePath::CharType kIwaDirName[] =
    FILE_PATH_LITERAL("iwa");
inline constexpr base::FilePath::CharType kMainSwbnFileName[] =
    FILE_PATH_LITERAL("main.swbn");

struct InstalledBundle {
  bool operator==(const InstalledBundle& other) const;
  bool operator!=(const InstalledBundle& other) const;

  base::FilePath path;
};

struct DevModeBundle {
  bool operator==(const DevModeBundle& other) const;
  bool operator!=(const DevModeBundle& other) const;

  base::FilePath path;
};

struct DevModeProxy {
  bool operator==(const DevModeProxy& other) const;
  bool operator!=(const DevModeProxy& other) const;

  url::Origin proxy_url;
};

using IsolatedWebAppLocation =
    absl::variant<InstalledBundle, DevModeBundle, DevModeProxy>;

base::Value IsolatedWebAppLocationAsDebugValue(
    const IsolatedWebAppLocation& location);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_LOCATION_H_
