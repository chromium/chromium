// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MODEL_APP_INSTALLED_BY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MODEL_APP_INSTALLED_BY_H_

#include <deque>
#include <optional>

#include "base/time/time.h"
#include "url/gurl.h"

namespace base {
class DictValue;
}  // namespace base

namespace web_app {

namespace proto {
class InstalledBy;
}

// Represents information about when and from where an app was installed via the
// Web Install API.
class AppInstalledBy {
 public:
  // Creates an AppInstalledBy with the given install time and requesting URL.
  // CHECK-fails if |install_time| is null (zero) or |requesting_url| is
  // invalid.
  AppInstalledBy(base::Time install_api_call_time, GURL requesting_url);

  AppInstalledBy(const AppInstalledBy&);
  AppInstalledBy& operator=(const AppInstalledBy&);
  AppInstalledBy(AppInstalledBy&&);
  AppInstalledBy& operator=(AppInstalledBy&&);
  ~AppInstalledBy();

  bool operator==(const AppInstalledBy&) const = default;

  // Parses an AppInstalledBy from a proto::InstalledBy message.
  // Returns std::nullopt if:
  // - Either proto field (install_time or requesting_url) is not set
  // - The install_time is 0 (null time)
  // - The requesting_url is empty or invalid
  static std::optional<AppInstalledBy> Parse(const proto::InstalledBy& proto);

  // Serializes this AppInstalledBy to a proto::InstalledBy message.
  proto::InstalledBy ToProto() const;

  // Converts this AppInstalledBy to a debug value for display.
  base::DictValue InstalledByToDebugValue() const;

  // Returns the time when the installation was attempted.
  const base::Time& install_api_call_time() const {
    return install_api_call_time_;
  }

  // Returns the URL of the page that attempted to install this app via the Web
  // Install API.
  const GURL& requesting_url() const { return requesting_url_; }

 private:
  base::Time install_api_call_time_;
  GURL requesting_url_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MODEL_APP_INSTALLED_BY_H_
