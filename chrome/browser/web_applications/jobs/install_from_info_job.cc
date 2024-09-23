// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/install_from_info_job.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/jobs/uninstall/web_app_uninstall_and_replace_job.h"
#include "chrome/browser/web_applications/proto/web_app_proto_package.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

InstallFromInfoJob::InstallFromInfoJob(
    Profile* profile,
    base::Value::Dict& debug_value,
    std::unique_ptr<WebAppInstallInfo> install_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_surface,
    std::optional<WebAppInstallParams> install_params,
    ResultCallback install_callback)
    : profile_(*profile),
      debug_value_(debug_value),
      manifest_id_(install_info->manifest_id()),
      app_id_(
          GenerateAppIdFromManifestId(manifest_id_,
                                      install_info->parent_app_manifest_id)),
      overwrite_existing_manifest_fields_(overwrite_existing_manifest_fields),
      install_surface_(install_surface),
      install_params_(std::move(install_params)),
      install_info_(std::move(install_info)),
      callback_(std::move(install_callback)) {
  if (install_params_.has_value() &&
      install_params_->install_state !=
          proto::InstallState::INSTALLED_WITH_OS_INTEGRATION) {
    CHECK(!install_params_->add_to_applications_menu);
    CHECK(!install_params_->add_to_desktop);
    CHECK(!install_params_->add_to_quick_launch_bar);
  }

  debug_value_->Set("app_id", app_id_);
  debug_value_->Set("start_url", install_info_->start_url().spec());
  debug_value_->Set("overwrite_existing_manifest_fields",
                    overwrite_existing_manifest_fields_);
  debug_value_->Set("install_surface", static_cast<int>(install_surface_));
  debug_value_->Set("has_install_params", !!install_params_);
}

InstallFromInfoJob::~InstallFromInfoJob() = default;

void InstallFromInfoJob::Start(WithAppResources* lock_with_app_resources) {
  CHECK(lock_with_app_resources);

  lock_with_app_resources_ = lock_with_app_resources;
  PopulateProductIcons(install_info_.get(),
                       /*icons_map=*/nullptr);
  // No IconsMap to populate shortcut item icons from.

  if (install_params_.has_value()) {
    ApplyParamsToWebAppInstallInfo(*install_params_, *install_info_);
  }

  if (webapps::InstallableMetrics::IsReportableInstallSource(
          install_surface_)) {
    webapps::InstallableMetrics::TrackInstallEvent(install_surface_);
  }

  WebAppInstallFinalizer::FinalizeOptions options(install_surface_);
  options.overwrite_existing_manifest_fields =
      overwrite_existing_manifest_fields_;

  if (install_params_.has_value()) {
    ApplyParamsToFinalizeOptions(*install_params_, options);
  } else {
    options.install_state =
        proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION;
    options.add_to_applications_menu = false;
    options.add_to_quick_launch_bar = false;
    options.add_to_desktop = false;
  }

  debug_value_->Set("options.install_state",
                    base::ToString(options.install_state));
  lock_with_app_resources_->install_finalizer().FinalizeInstall(
      *install_info_, options,
      base::BindOnce(&InstallFromInfoJob::OnInstallCompleted,
                     weak_factory_.GetWeakPtr()));
}

void InstallFromInfoJob::OnInstallCompleted(const webapps::AppId& app_id,
                                            webapps::InstallResultCode code) {
  debug_value_->Set("result_code", base::ToString(code));
  CHECK(callback_);
  std::move(callback_).Run(app_id_, code);
}

}  // namespace web_app
