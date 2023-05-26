// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/uninstall_request.h"

#include "base/values.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {

UninstallRequest::UninstallRequest(
    webapps::WebappUninstallSource uninstall_source,
    AppId app_id,
    absl::optional<WebAppManagement::Type> install_source,
    absl::optional<GURL> install_url)
    : uninstall_source_(uninstall_source),
      app_id_(std::move(app_id)),
      install_source_(std::move(install_source)),
      install_url_(std::move(install_url)) {
  CheckFields();
}

UninstallRequest::UninstallRequest(
    base::PassKey<WebAppUninstallCommand>,
    bool is_sub_request,
    webapps::WebappUninstallSource uninstall_source,
    AppId app_id,
    absl::optional<WebAppManagement::Type> install_source,
    absl::optional<GURL> install_url)
    : uninstall_source_(uninstall_source),
      app_id_(std::move(app_id)),
      install_source_(std::move(install_source)),
      install_url_(std::move(install_url)),
      is_sub_request_(is_sub_request) {
  CheckFields();
}

UninstallRequest::UninstallRequest(UninstallRequest&&) = default;

UninstallRequest::~UninstallRequest() = default;

UninstallRequest& UninstallRequest::operator=(UninstallRequest&&) = default;

base::Value UninstallRequest::ToDebugValue() const {
  base::Value::Dict dict;
  dict.Set("uninstall_source", base::ToString(uninstall_source_));
  dict.Set("app_id", app_id_);
  dict.Set("install_source", install_source_
                                 ? base::Value(base::ToString(*install_source_))
                                 : base::Value());
  dict.Set("install_url",
           install_url_ ? base::Value(install_url_->spec()) : base::Value());
  dict.Set("is_sub_request", is_sub_request_);
  return base::Value(std::move(dict));
}

void UninstallRequest::CheckFields() const {
  CHECK(!app_id_.empty());
  if (install_url_) {
    CHECK(install_source_);
  }
  if (install_source_ == WebAppManagement::Type::kSync) {
    CHECK(!install_url_);
  }
}

}  // namespace web_app
