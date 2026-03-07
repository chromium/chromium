// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_finalizer.h"

#include <map>
#include <optional>
#include <utility>

#include "ash/constants/web_app_id_constants.h"
#include "base/barrier_callback.h"
#include "base/check_is_test.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/jobs/finalize_install_job.h"
#include "chrome/browser/web_applications/jobs/finalize_update_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_url_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_web_app_job.h"
#include "chrome/browser/web_applications/model/app_installed_by.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcuts_menu.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/proto/web_app.equal.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_scope.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_translation_manager.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_data.h"
#endif

namespace web_app {
namespace {
}  // namespace

WebAppInstallFinalizer::WebAppInstallFinalizer(Profile* profile)
    : profile_(profile) {}

WebAppInstallFinalizer::~WebAppInstallFinalizer() = default;

void WebAppInstallFinalizer::FinalizeInstall(
    const WebAppInstallInfo& web_app_info,
    const FinalizeJobOptions& options,
    InstallFinalizedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/40693380): Implement a before-start queue in
  // WebAppInstallManager and replace this runtime error in
  // WebAppInstallFinalizer with DCHECK(started_).
  if (!started_) {
    std::move(callback).Run(
        webapps::AppId(), webapps::InstallResultCode::kWebAppProviderNotReady);
    return;
  }

  std::unique_ptr<FinalizeInstallJob> web_app_install_job =
      std::make_unique<FinalizeInstallJob>(*profile_, nullptr, nullptr,
                                           std::move(web_app_info), options);
  FinalizeInstallJob* job_ptr = web_app_install_job.get();
  install_jobs_.insert(std::move(web_app_install_job));
  job_ptr->Start(base::BindOnce(&WebAppInstallFinalizer::OnInstallJobFinished,
                                weak_ptr_factory_.GetWeakPtr(),
                                base::Unretained(job_ptr),
                                std::move(callback)));
}

void WebAppInstallFinalizer::OnInstallJobFinished(
    FinalizeInstallJob* job,
    InstallFinalizedCallback callback,
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  std::move(callback).Run(app_id, code);
  install_jobs_.erase(job);
}

void WebAppInstallFinalizer::OnInstallUpdateJobFinished(
    FinalizeUpdateJob* job,
    InstallFinalizedCallback callback,
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  std::move(callback).Run(app_id, code);
  install_update_jobs_.erase(job);
}

void WebAppInstallFinalizer::FinalizeUpdate(
    const WebAppInstallInfo& web_app_info,
    InstallFinalizedCallback callback) {
  FinalizeUpdate(nullptr, web_app_info, std::move(callback));
}

void WebAppInstallFinalizer::FinalizeUpdate(
    WithAppResources* lock,
    const WebAppInstallInfo& web_app_info,
    InstallFinalizedCallback callback) {
  std::unique_ptr<FinalizeUpdateJob> web_app_install_update_job =
      std::make_unique<FinalizeUpdateJob>(nullptr, lock, *provider_,
                                          web_app_info);
  FinalizeUpdateJob* job_ptr = web_app_install_update_job.get();
  install_update_jobs_.insert(std::move(web_app_install_update_job));
  job_ptr->Start(
      base::BindOnce(&WebAppInstallFinalizer::OnInstallUpdateJobFinished,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(job_ptr),
                     std::move(callback)));
}

void WebAppInstallFinalizer::SetProvider(base::PassKey<WebAppProvider>,
                                         WebAppProvider& provider) {
  provider_ = &provider;
}

void WebAppInstallFinalizer::Start() {
  DCHECK(!started_);
  started_ = true;
}

void WebAppInstallFinalizer::Shutdown() {
  started_ = false;
  // TODO(crbug.com/40810770): Turn WebAppInstallFinalizer into a command so it
  // can properly call callbacks on shutdown instead of dropping them on
  // shutdown.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void WebAppInstallFinalizer::SetClockForTesting(base::Clock* clock) {
  if (provider_) {
    provider_->SetClockForTesting(clock);
  }
}

}  // namespace web_app
