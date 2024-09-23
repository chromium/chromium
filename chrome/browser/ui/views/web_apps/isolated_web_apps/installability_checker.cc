// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/installability_checker.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/check_isolated_web_app_bundle_installability_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"

namespace web_app {

// static
std::unique_ptr<InstallabilityChecker> InstallabilityChecker::CreateAndStart(
    Profile* profile,
    WebAppProvider* web_app_provider,
    IwaSourceBundleWithMode source,
    base::OnceCallback<void(Result)> callback) {
  std::unique_ptr<InstallabilityChecker> checker =
      base::WrapUnique(new InstallabilityChecker(profile, web_app_provider,
                                                 std::move(callback)));
  checker->Start(std::move(source));
  return checker;
}

InstallabilityChecker::~InstallabilityChecker() = default;

InstallabilityChecker::InstallabilityChecker(
    Profile* profile,
    WebAppProvider* web_app_provider,
    base::OnceCallback<void(Result)> callback)
    : profile_(profile),
      web_app_provider_(web_app_provider),
      callback_(std::move(callback)) {
  CHECK(profile_);
  CHECK(web_app_provider_);
}

void InstallabilityChecker::Start(IwaSourceBundleWithMode source) {
  IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppSource(
      source, base::BindOnce(&InstallabilityChecker::OnLoadedUrlInfo,
                             weak_ptr_factory_.GetWeakPtr(), source));
}

void InstallabilityChecker::OnLoadedUrlInfo(
    IwaSourceBundleWithMode source,
    base::expected<IsolatedWebAppUrlInfo, std::string> url_info) {
  if (!url_info.has_value()) {
    std::move(callback_).Run(BundleInvalid{url_info.error()});
    return;
  }
  SignedWebBundleMetadata::Create(
      profile_, web_app_provider_, url_info.value(), source,
      base::BindOnce(&InstallabilityChecker::OnLoadedMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InstallabilityChecker::OnLoadedMetadata(
    base::expected<SignedWebBundleMetadata, std::string> metadata) {
  if (!metadata.has_value()) {
    std::move(callback_).Run(BundleInvalid{metadata.error()});
    return;
  }
  web_app_provider_->scheduler().CheckIsolatedWebAppBundleInstallability(
      metadata.value(),
      base::BindOnce(&InstallabilityChecker::OnInstallabilityChecked,
                     weak_ptr_factory_.GetWeakPtr(), metadata.value()));
}

void InstallabilityChecker::OnInstallabilityChecked(
    SignedWebBundleMetadata metadata,
    IsolatedInstallabilityCheckResult installability_check_result,
    std::optional<base::Version> installed_version) {
  switch (installability_check_result) {
    case IsolatedInstallabilityCheckResult::kInstallable:
      std::move(callback_).Run(BundleInstallable{metadata});
      return;
    case IsolatedInstallabilityCheckResult::kUpdatable:
      CHECK(installed_version.has_value());
      std::move(callback_).Run(
          BundleUpdatable{metadata, installed_version.value()});
      return;
    case IsolatedInstallabilityCheckResult::kOutdated:
      CHECK(installed_version.has_value());
      std::move(callback_).Run(
          BundleOutdated{metadata, installed_version.value()});
      return;
    case IsolatedInstallabilityCheckResult::kShutdown:
      std::move(callback_).Run(ProfileShutdown{});
      return;
  }
}

}  // namespace web_app
