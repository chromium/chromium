// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_mover.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_features.h"

namespace {

bool g_disabled_for_testing = false;
bool g_skip_wait_for_sync_for_testing = false;

base::OnceClosure& GetCompletedCallbackForTesting() {
  static base::NoDestructor<base::OnceClosure> callback;
  return *callback;
}

}  // namespace

namespace web_app {

std::unique_ptr<WebAppMover> WebAppMover::CreateIfNeeded(Profile* profile) {
  DCHECK(base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions));

  if (g_disabled_for_testing)
    return nullptr;

  if (!base::FeatureList::IsEnabled(features::kMoveWebApp))
    return nullptr;

  std::string uninstall_url_prefix_str =
      features::kMoveWebAppUninstallStartUrlPrefix.Get();
  std::string install_url_str = features::kMoveWebAppInstallStartUrl.Get();
  if (uninstall_url_prefix_str.empty() || install_url_str.empty())
    return nullptr;

  GURL uninstall_url_prefix = GURL(uninstall_url_prefix_str);
  GURL install_url = GURL(install_url_str);
  // The URLs have to be valid, and the installation URL cannot be contained in
  // the uninstall prefix.
  if (!uninstall_url_prefix.is_valid() || !install_url.is_valid() ||
      base::StartsWith(install_url.spec(), uninstall_url_prefix.spec())) {
    return nullptr;
  }

  return std::make_unique<WebAppMover>(profile, uninstall_url_prefix,
                                       install_url);
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
                         const GURL& uninstall_url_prefix,
                         const GURL& install_url)
    : profile_(profile),
      uninstall_url_prefix_(uninstall_url_prefix),
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
}

void WebAppMover::OnSyncCycleCompleted(syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);
  if (sync_ready_callback_)
    std::move(sync_ready_callback_).Run();
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
  // TODO(dmurph): Implement migration here.

  if (GetCompletedCallbackForTesting())
    std::move(GetCompletedCallbackForTesting()).Run();
}

}  // namespace web_app