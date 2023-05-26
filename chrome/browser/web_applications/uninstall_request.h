// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_UNINSTALL_REQUEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_UNINSTALL_REQUEST_H_

#include "base/types/pass_key.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace web_app {

class WebAppUninstallCommand;

// This represents requests for different tiers of uninstallation:
// - app_id only: App removal.
// - app_id + install_source: Install source removal.
// - app_id + install_source + install_url: Install URL removal.
// Install sources are always removed if there are no remaining install URLs.
// Apps are always uninstalled if there are no remaining install sources.
class UninstallRequest {
 public:
  UninstallRequest(
      webapps::WebappUninstallSource uninstall_source,
      AppId app_id,
      absl::optional<WebAppManagement::Type> install_source = absl::nullopt,
      absl::optional<GURL> install_url = absl::nullopt);
  UninstallRequest(
      base::PassKey<WebAppUninstallCommand>,
      bool is_sub_request,
      webapps::WebappUninstallSource uninstall_source,
      AppId app_id,
      absl::optional<WebAppManagement::Type> install_source = absl::nullopt,
      absl::optional<GURL> install_url = absl::nullopt);
  UninstallRequest(const UninstallRequest&) = delete;
  UninstallRequest(UninstallRequest&&);
  ~UninstallRequest();

  UninstallRequest& operator=(const UninstallRequest&) = delete;
  UninstallRequest& operator=(UninstallRequest&&);

  webapps::WebappUninstallSource uninstall_source() const {
    return uninstall_source_;
  }
  AppId app_id() const { return app_id_; }
  absl::optional<WebAppManagement::Type> install_source() const {
    return install_source_;
  }
  absl::optional<GURL> install_url() const { return install_url_; }
  bool is_sub_request() const { return is_sub_request_; }

  base::Value ToDebugValue() const;

 private:
  void CheckFields() const;

  webapps::WebappUninstallSource uninstall_source_;
  AppId app_id_;
  absl::optional<WebAppManagement::Type> install_source_;
  absl::optional<GURL> install_url_;
  bool is_sub_request_ = false;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_UNINSTALL_REQUEST_H_
