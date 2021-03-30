// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_mover.h"

#include "base/barrier_closure.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

bool g_disabled_for_testing = false;
bool g_skip_wait_for_sync_for_testing = false;

base::OnceClosure& GetCompletedCallbackForTesting() {
  static base::NoDestructor<base::OnceClosure> callback;
  return *callback;
}

}  // namespace

namespace web_app {

std::unique_ptr<WebAppMover> WebAppMover::CreateIfNeeded(
    Profile* profile,
    AppRegistrar* registrar,
    InstallFinalizer* install_finalizer,
    InstallManager* install_manager,
    AppRegistryController* controller) {
  DCHECK(base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions));
  if (g_disabled_for_testing)
    return nullptr;

  if (!base::FeatureList::IsEnabled(features::kMoveWebApp))
    return nullptr;

  std::string uninstall_url_prefix =
      features::kMoveWebAppUninstallStartUrlPrefix.Get();
  std::string uninstall_pattern_str =
      features::kMoveWebAppUninstallStartUrlPattern.Get();
  std::string install_url_str = features::kMoveWebAppInstallStartUrl.Get();

  // Only continue if exactly one of the uninstall settings is set, and the
  // install url is set.
  if (install_url_str.empty()) {
    base::UmaHistogramEnumeration("WebApp.Mover.Result",
                                  WebAppMoverResult::kInvalidConfiguration);
    return nullptr;
  }

  GURL install_url = GURL(install_url_str);

  // |install_url| has to be a valid URL.
  if (!install_url.is_valid()) {
    base::UmaHistogramEnumeration("WebApp.Mover.Result",
                                  WebAppMoverResult::kInvalidConfiguration);
    return nullptr;
  }

  WebAppMover::UninstallMode uninstall_mode;
  std::string uninstall_prefix_or_pattern;
  if (!uninstall_url_prefix.empty()) {
    DCHECK(uninstall_pattern_str.empty());
    // The installation URL cannot be contained in the uninstall prefix.
    if (base::StartsWith(install_url.spec(), uninstall_url_prefix)) {
      base::UmaHistogramEnumeration("WebApp.Mover.Result",
                                    WebAppMoverResult::kInvalidConfiguration);
      return nullptr;
    }
    uninstall_mode = WebAppMover::UninstallMode::kPrefix;
    uninstall_prefix_or_pattern = uninstall_url_prefix;
  } else if (!uninstall_pattern_str.empty()) {
    re2::RE2 uninstall_pattern(uninstall_pattern_str);
    // The pattern must be valid, and the install URL must not match the
    // pattern.
    if (uninstall_pattern.error_code() != re2::RE2::NoError ||
        re2::RE2::FullMatch(install_url.spec(), uninstall_pattern)) {
      base::UmaHistogramEnumeration("WebApp.Mover.Result",
                                    WebAppMoverResult::kInvalidConfiguration);
      return nullptr;
    }
    uninstall_mode = WebAppMover::UninstallMode::kPattern;
    uninstall_prefix_or_pattern = uninstall_pattern_str;
  } else {
    base::UmaHistogramEnumeration("WebApp.Mover.Result",
                                  WebAppMoverResult::kInvalidConfiguration);
    return nullptr;
  }

  return std::make_unique<WebAppMover>(
      profile, registrar, install_finalizer, install_manager, controller,
      uninstall_mode, uninstall_prefix_or_pattern, install_url);
}

void WebAppMover::DisableForTesting() {
  g_disabled_for_testing = true;
}

void WebAppMover::SkipWaitForSyncForTesting() {
  g_skip_wait_for_sync_for_testing = true;
}

void WebAppMover::SetCompletedCallbackForTesting(base::OnceClosure callback) {
  GetCompletedCallbackForTesting() = std::move(callback);
}

WebAppMover::WebAppMover(Profile* profile,
                         AppRegistrar* registrar,
                         InstallFinalizer* install_finalizer,
                         InstallManager* install_manager,
                         AppRegistryController* controller,
                         UninstallMode uninstall_mode,
                         std::string uninstall_url_prefix_or_pattern,
                         const GURL& install_url)
    : profile_(profile),
      registrar_(registrar),
      install_finalizer_(install_finalizer),
      install_manager_(install_manager),
      controller_(controller),
      uninstall_mode_(uninstall_mode),
      uninstall_url_prefix_or_pattern_(uninstall_url_prefix_or_pattern),
      install_url_(install_url) {}

WebAppMover::~WebAppMover() = default;

void WebAppMover::Start() {
  // We cannot grab the SyncService in the constructor without creating a
  // circular KeyedService dependency.
  sync_service_ = ProfileSyncServiceFactory::GetForProfile(profile_);
  // This can be a nullptr if the --disable-sync switch is specified.
  if (sync_service_)
    sync_observer_.Observe(sync_service_);
  // We must wait for sync to complete at least one cycle (if it is turned on).
  // This avoids our local updates accidentally re-installing any web apps that
  // were uninstalled on other devices. Installing the replacement app will send
  // that record to sync servers, and if the user had uninstalled the 'source'
  // app on another computer, we could miss that message and accidentally end up
  // with the 'destination' app installed when it shouldn't have been installed
  // in the first place (as the user uninstalled the 'source' app).
  WaitForFirstSyncCycle(base::BindOnce(&WebAppMover::OnFirstSyncCycleComplete,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void WebAppMover::Shutdown() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  sync_observer_.Reset();
  migration_keep_alive_.reset();
}

void WebAppMover::OnSyncCycleCompleted(syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);
  if (sync_ready_callback_)
    std::move(sync_ready_callback_).Run();
  // Only the first cycle cycle matters, as this triggers the WebAppMover logic,
  // and |sync_ready_callback_| is never set again. Thus we can stop observing.
  sync_observer_.Reset();
}

void WebAppMover::OnSyncShutdown(syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);
  sync_observer_.Reset();
  sync_service_ = nullptr;
}

void WebAppMover::WaitForFirstSyncCycle(base::OnceClosure callback) {
  DCHECK(!sync_ready_callback_);
  if (g_skip_wait_for_sync_for_testing || !sync_service_ ||
      sync_service_->HasCompletedSyncCycle() ||
      !sync_service_->IsSyncFeatureEnabled()) {
    std::move(callback).Run();
    return;
  }
  sync_ready_callback_ = std::move(callback);
}

void WebAppMover::OnFirstSyncCycleComplete() {
  DCHECK(apps_to_uninstall_.empty());

  base::ScopedClosureRunner complete_callback_runner;
  if (GetCompletedCallbackForTesting()) {
    complete_callback_runner.ReplaceClosure(
        std::move(GetCompletedCallbackForTesting()));
  }

  for (const AppId& id : registrar_->GetAppIds()) {
    // Stop if the destination app is already installed.
    const GURL& start_url = registrar_->GetAppStartUrl(id);
    if (start_url == install_url_) {
      RecordResults(WebAppMoverResult::kInstallAppExists);
      return;
    }
    // To avoid edge cases only consider installed apps to uninstall.
    if (!registrar_->IsInstalled(id))
      continue;
    switch (uninstall_mode_) {
      case UninstallMode::kPattern:
        if (re2::RE2::FullMatch(start_url.spec(),
                                uninstall_url_prefix_or_pattern_)) {
          apps_to_uninstall_.push_back(id);
          new_app_open_as_window_ =
              registrar_->GetAppUserDisplayMode(id) == DisplayMode::kStandalone;
        }
        break;
      case UninstallMode::kPrefix:
        if (base::StartsWith(start_url.spec(),
                             uninstall_url_prefix_or_pattern_)) {
          apps_to_uninstall_.push_back(id);
          new_app_open_as_window_ =
              registrar_->GetAppUserDisplayMode(id) == DisplayMode::kStandalone;
        }
    }
  }

  if (apps_to_uninstall_.empty()) {
    RecordResults(WebAppMoverResult::kNoAppsToUninstall);
    return;
  }

  install_manager_->LoadWebAppAndCheckManifest(
      install_url_, webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      base::BindOnce(&WebAppMover::OnInstallManifestFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(complete_callback_runner)));
}

void WebAppMover::OnInstallManifestFetched(
    base::ScopedClosureRunner complete_callback_runner,
    std::unique_ptr<content::WebContents> web_contents,
    InstallManager::InstallableCheckResult result,
    base::Optional<AppId> app_id) {
  switch (result) {
    case InstallManager::InstallableCheckResult::kAlreadyInstalled:
      LOG(WARNING) << "App already installed.";
      return;
    case InstallManager::InstallableCheckResult::kNotInstallable:
      // If the app is not installable, then abort.
      RecordResults(WebAppMoverResult::kNotInstallable);
      return;
    case InstallManager::InstallableCheckResult::kInstallable:
      break;
  }
  DCHECK(!apps_to_uninstall_.empty());

  migration_keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::APP_START_URL_MIGRATION,
      KeepAliveRestartOption::DISABLED);

  scoped_refptr<base::RefCountedData<bool>> success_accumulator =
      base::MakeRefCounted<base::RefCountedData<bool>>(true);

  auto barrier = base::BarrierClosure(
      apps_to_uninstall_.size(),
      base::BindOnce(&WebAppMover::OnAllUninstalled,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(complete_callback_runner),
                     std::move(web_contents), success_accumulator));
  for (const AppId& id : apps_to_uninstall_) {
    install_finalizer_->UninstallExternalAppByUser(
        id,
        base::BindOnce(
            [](base::OnceClosure done,
               scoped_refptr<base::RefCountedData<bool>> success_accumulator,
               bool success) {
              if (!success) {
                LOG(WARNING)
                    << "Uninstallation unsuccesful in app move operation.";
                success_accumulator->data = false;
              }
              std::move(done).Run();
            },
            barrier, success_accumulator));
  }
}

void WebAppMover::OnAllUninstalled(
    base::ScopedClosureRunner complete_callback_runner,
    std::unique_ptr<content::WebContents> web_contents_for_install,
    scoped_refptr<base::RefCountedData<bool>> success_accumulator) {
  if (!success_accumulator->data) {
    RecordResults(WebAppMoverResult::kUninstallFailure);
    return;
  }
  auto* web_contents = web_contents_for_install.get();
  install_manager_->InstallWebAppFromManifest(
      web_contents, true, webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      base::BindOnce([](content::WebContents* initiator_web_contents,
                        std::unique_ptr<WebApplicationInfo> web_app_info,
                        ForInstallableSite for_installable_site,
                        InstallManager::WebAppInstallationAcceptanceCallback
                            acceptance_callback) {
        // Note: |open_as_window| is set to false here (which it should be by
        // default), because if that is true the WebAppInstallTask will try to
        // reparent the the web contents into an app browser. This is
        // impossible, as this web contents is internal & not visible to the
        // user (and we will segfault). Instead, set the user display mode after
        // installation is complete.
        DCHECK(web_app_info);
        web_app_info->open_as_window = false;
        std::move(acceptance_callback).Run(true, std::move(web_app_info));
      }),
      base::BindOnce(&WebAppMover::OnInstallCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(complete_callback_runner),
                     std::move(web_contents_for_install)));
}

void WebAppMover::OnInstallCompleted(
    base::ScopedClosureRunner complete_callback_runner,
    std::unique_ptr<content::WebContents> web_contents_for_install,
    const AppId& id,
    InstallResultCode code) {
  if (code == InstallResultCode::kSuccessNewInstall) {
    if (new_app_open_as_window_)
      controller_->SetAppUserDisplayMode(id, DisplayMode::kStandalone, false);
    RecordResults(WebAppMoverResult::kSuccess);
  } else {
    LOG(WARNING) << "Installation in app move operation failed: " << code;
    RecordResults(WebAppMoverResult::kInstallFailure);
  }
  migration_keep_alive_.reset();
}

void WebAppMover::RecordResults(WebAppMoverResult result) {
  if (results_recorded_)
    return;
  results_recorded_ = true;
  base::UmaHistogramEnumeration("WebApp.Mover.Result", result);
}

}  // namespace web_app
