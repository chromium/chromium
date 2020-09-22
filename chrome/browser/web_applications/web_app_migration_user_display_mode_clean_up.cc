// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_migration_user_display_mode_clean_up.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace {

bool g_disabled_for_testing = false;
bool g_skip_wait_for_sync_for_testing = false;

base::OnceClosure& GetCompletedCallbackForTesting() {
  static base::NoDestructor<base::OnceClosure> callback;
  return *callback;
}

}  // namespace

namespace web_app {

void WebAppMigrationUserDisplayModeCleanUp::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kWebAppsUserDisplayModeCleanedUp, false);
}

std::unique_ptr<WebAppMigrationUserDisplayModeCleanUp>
WebAppMigrationUserDisplayModeCleanUp::CreateIfNeeded(
    Profile* profile,
    WebAppSyncBridge* sync_bridge) {
  DCHECK(base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions));

  if (g_disabled_for_testing)
    return nullptr;

  if (profile->GetPrefs()->GetBoolean(prefs::kWebAppsUserDisplayModeCleanedUp))
    return nullptr;

  if (!base::FeatureList::IsEnabled(
          features::kDesktopPWAsMigrationUserDisplayModeCleanUp)) {
    // Clear the pref if clean up is disabled to allow it to run again if it
    // gets enabled again.
    profile->GetPrefs()->SetBoolean(prefs::kWebAppsUserDisplayModeCleanedUp,
                                    false);
    return nullptr;
  }

  return std::make_unique<WebAppMigrationUserDisplayModeCleanUp>(profile,
                                                                 sync_bridge);
}

void WebAppMigrationUserDisplayModeCleanUp::DisableForTesting() {
  g_disabled_for_testing = true;
}

void WebAppMigrationUserDisplayModeCleanUp::SkipWaitForSyncForTesting() {
  g_skip_wait_for_sync_for_testing = true;
}

void WebAppMigrationUserDisplayModeCleanUp::SetCompletedCallbackForTesting(
    base::OnceClosure callback) {
  GetCompletedCallbackForTesting() = std::move(callback);
}

WebAppMigrationUserDisplayModeCleanUp::WebAppMigrationUserDisplayModeCleanUp(
    Profile* profile,
    WebAppSyncBridge* sync_bridge)
    : profile_(profile),
      sync_bridge_(sync_bridge),
      bookmark_app_registrar_(profile),
      bookmark_app_registry_controller_(profile, &bookmark_app_registrar_) {}

WebAppMigrationUserDisplayModeCleanUp::
    ~WebAppMigrationUserDisplayModeCleanUp() = default;

void WebAppMigrationUserDisplayModeCleanUp::Start() {
  // We cannot grab the SyncService in the constructor without creating a
  // circular KeyedService dependency.
  sync_service_ = ProfileSyncServiceFactory::GetForProfile(profile_);
  if (sync_service_)
    sync_observer_.Add(sync_service_);
  bookmark_app_registry_controller_.Init(base::BindOnce(
      &WebAppMigrationUserDisplayModeCleanUp::OnBookmarkAppRegistryReady,
      weak_ptr_factory_.GetWeakPtr()));
}

void WebAppMigrationUserDisplayModeCleanUp::Shutdown() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  sync_observer_.RemoveAll();
}

void WebAppMigrationUserDisplayModeCleanUp::OnSyncCycleCompleted(
    syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);
  if (sync_ready_callback_)
    std::move(sync_ready_callback_).Run();
}

void WebAppMigrationUserDisplayModeCleanUp::OnSyncShutdown(
    syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);
  sync_observer_.RemoveAll();
  sync_service_ = nullptr;
}

void WebAppMigrationUserDisplayModeCleanUp::OnBookmarkAppRegistryReady() {
  // We must wait for sync to complete at least one cycle.
  // This avoids our local updates accidentally re-installing any web apps that
  // were uninstalled on other devices. Updating any synced web app field sends
  // the entire web app to the sync server. If that web app had been removed
  // since we last synced then it would look like a new installation and get
  // reinstalled on all the user's synced devices.
  WaitForFirstSyncCycle(base::BindOnce(
      &WebAppMigrationUserDisplayModeCleanUp::OnFirstSyncCycleComplete,
      weak_ptr_factory_.GetWeakPtr()));
}

void WebAppMigrationUserDisplayModeCleanUp::WaitForFirstSyncCycle(
    base::OnceClosure callback) {
  DCHECK(!sync_ready_callback_);
  if (g_skip_wait_for_sync_for_testing || !sync_service_ ||
      sync_service_->HasCompletedSyncCycle()) {
    std::move(callback).Run();
    return;
  }
  sync_ready_callback_ = std::move(callback);
}

void WebAppMigrationUserDisplayModeCleanUp::OnFirstSyncCycleComplete() {
  std::vector<AppId> clean_up_ids = bookmark_app_registrar_.GetAppIds();

  // Filter down to apps that have a windowed bookmark app and a browser tab BMO
  // app.
  base::EraseIf(clean_up_ids, [this](const AppId& app_id) {
    const WebApp* web_app = sync_bridge_->registrar().GetAppById(app_id);
    if (!web_app)
      return true;

    if (bookmark_app_registrar_.GetAppUserDisplayModeForMigration(app_id) ==
        DisplayMode::kBrowser) {
      return true;
    }

    return web_app->user_display_mode() != DisplayMode::kBrowser;
  });

  {
    ScopedRegistryUpdate update(sync_bridge_);
    for (const AppId& app_id : clean_up_ids) {
      update->UpdateApp(app_id)->SetUserDisplayMode(
          bookmark_app_registrar_.GetAppUserDisplayModeForMigration(app_id));
    }
  }

  profile_->GetPrefs()->SetBoolean(prefs::kWebAppsUserDisplayModeCleanedUp,
                                   true);
  base::UmaHistogramBoolean("WebApp.Migration.UserDisplayModeCleanUp",
                            /*BooleanMigrated=*/!clean_up_ids.empty());

  if (GetCompletedCallbackForTesting())
    std::move(GetCompletedCallbackForTesting()).Run();
}

}  // namespace web_app
