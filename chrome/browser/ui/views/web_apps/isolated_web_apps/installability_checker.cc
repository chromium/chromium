// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/installability_checker.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/check_isolated_web_app_bundle_installability_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

namespace {
using InstallabilityCheckResult =
    CheckIsolatedWebAppBundleInstallabilityCommand::InstallabilityCheckResult;
}  // namespace

// static
std::unique_ptr<InstallabilityChecker> InstallabilityChecker::CreateAndStart(
    Profile* profile,
    WebAppProvider* web_app_provider,
    const base::FilePath& bundle_path,
    Delegate* delegate) {
  std::unique_ptr<InstallabilityChecker> checker = base::WrapUnique(
      new InstallabilityChecker(profile, web_app_provider, delegate));
  checker->Start(bundle_path);
  return checker;
}

InstallabilityChecker::~InstallabilityChecker() = default;

InstallabilityChecker::InstallabilityChecker(Profile* profile,
                                             WebAppProvider* web_app_provider,
                                             Delegate* delegate)
    : profile_(profile),
      web_app_provider_(web_app_provider),
      delegate_(delegate) {
  CHECK(profile_);
  CHECK(web_app_provider_);
  CHECK(delegate_);
}

void InstallabilityChecker::Start(const base::FilePath& bundle_path) {
  IsolatedWebAppLocation location = DevModeBundle{bundle_path};
  IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppLocation(
      location, base::BindOnce(&InstallabilityChecker::OnLoadedUrlInfo,
                               weak_ptr_factory_.GetWeakPtr(), location));
}

void InstallabilityChecker::OnLoadedUrlInfo(
    IsolatedWebAppLocation location,
    base::expected<IsolatedWebAppUrlInfo, std::string> url_info) {
  if (!url_info.has_value()) {
    delegate_->OnBundleInvalid(url_info.error());
    return;
  }
  SignedWebBundleMetadata::Create(
      profile_, web_app_provider_, url_info.value(), location,
      base::BindOnce(&InstallabilityChecker::OnLoadedMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InstallabilityChecker::OnLoadedMetadata(
    base::expected<SignedWebBundleMetadata, std::string> metadata) {
  if (!metadata.has_value()) {
    delegate_->OnBundleInvalid(metadata.error());
    return;
  }
  web_app_provider_->scheduler().CheckIsolatedWebAppBundleInstallability(
      metadata.value(),
      base::BindOnce(&InstallabilityChecker::OnInstallabilityChecked,
                     weak_ptr_factory_.GetWeakPtr(), metadata.value()));
}

void InstallabilityChecker::OnInstallabilityChecked(
    SignedWebBundleMetadata metadata,
    InstallabilityCheckResult installability_check_result,
    absl::optional<base::Version> installed_version) {
  switch (installability_check_result) {
    case InstallabilityCheckResult::kInstallable:
      delegate_->OnBundleInstallable(metadata);
      return;
    case InstallabilityCheckResult::kUpdatable:
      CHECK(installed_version.has_value());
      delegate_->OnBundleUpdatable(metadata, installed_version.value());
      return;
    case InstallabilityCheckResult::kOutdated:
      CHECK(installed_version.has_value());
      delegate_->OnBundleOutdated(metadata, installed_version.value());
      return;
    case InstallabilityCheckResult::kShutdown:
      delegate_->OnProfileShutdown();
      return;
  }
}

}  // namespace web_app
