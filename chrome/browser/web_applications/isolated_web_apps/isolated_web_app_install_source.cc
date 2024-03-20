// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"

#include "base/files/file_path.h"
#include "base/strings/to_string.h"
#include "base/values.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

// static
IsolatedWebAppInstallSource IsolatedWebAppInstallSource::FromGraphicalInstaller(
    IwaSourceBundleWithModeAndFileOp source) {
  return IsolatedWebAppInstallSource(
      std::move(source), webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER);
}

// static
IsolatedWebAppInstallSource IsolatedWebAppInstallSource::FromExternalPolicy(
    IwaSourceProdModeWithFileOp source) {
  return IsolatedWebAppInstallSource(
      std::move(source), webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);
}

// static
IsolatedWebAppInstallSource IsolatedWebAppInstallSource::FromShimlessRma(
    IwaSourceProdModeWithFileOp source) {
  return IsolatedWebAppInstallSource(
      std::move(source), webapps::WebappInstallSource::IWA_SHIMLESS_RMA);
}

// static
IsolatedWebAppInstallSource IsolatedWebAppInstallSource::FromDevUi(
    IwaSourceDevModeWithFileOp source) {
  return IsolatedWebAppInstallSource(std::move(source),
                                     webapps::WebappInstallSource::IWA_DEV_UI);
}

// static
IsolatedWebAppInstallSource IsolatedWebAppInstallSource::FromDevCommandLine(
    IwaSourceDevModeWithFileOp source) {
  return IsolatedWebAppInstallSource(
      std::move(source), webapps::WebappInstallSource::IWA_DEV_COMMAND_LINE);
}

IsolatedWebAppInstallSource::IsolatedWebAppInstallSource(
    const IsolatedWebAppInstallSource&) = default;
IsolatedWebAppInstallSource& IsolatedWebAppInstallSource::operator=(
    const IsolatedWebAppInstallSource&) = default;

IsolatedWebAppInstallSource::~IsolatedWebAppInstallSource() = default;

base::Value IsolatedWebAppInstallSource::ToDebugValue() const {
  return base::Value(
      base::Value::Dict()
          .Set("source", source_.ToDebugValue())
          .Set("install_surface", base::ToString(install_surface_)));
}

IsolatedWebAppInstallSource::IsolatedWebAppInstallSource(
    IwaSourceWithModeAndFileOp source,
    webapps::WebappInstallSource install_surface)
    : source_(std::move(source)), install_surface_(install_surface) {}

}  // namespace web_app
