// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/bookmark_apps/bookmark_app_install_manager.h"
#include "chrome/browser/web_applications/bookmark_apps/external_web_apps.h"
#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/bookmark_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_ids_map.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

// static
WebAppProvider* WebAppProvider::Get(Profile* profile) {
  return WebAppProviderFactory::GetForProfile(profile);
}

// static
WebAppProvider* WebAppProvider::GetForWebContents(
    const content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile);
  return WebAppProvider::Get(profile);
}

WebAppProvider::WebAppProvider(Profile* profile) {
  if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions))
    CreateWebAppsSubsystems(profile);
  else
    CreateBookmarkAppsSubsystems(profile);
}

WebAppProvider::~WebAppProvider() = default;

void WebAppProvider::CreateWebAppsSubsystems(Profile* profile) {
  if (!AllowWebAppInstallation(profile))
    return;

  registrar_ = std::make_unique<WebAppRegistrar>();
  install_manager_ =
      std::make_unique<WebAppInstallManager>(profile, registrar_.get());
}

void WebAppProvider::CreateBookmarkAppsSubsystems(Profile* profile) {
  install_manager_ = std::make_unique<extensions::BookmarkAppInstallManager>();

  pending_app_manager_ =
      std::make_unique<extensions::PendingBookmarkAppManager>(profile);

  if (WebAppPolicyManager::ShouldEnableForProfile(profile)) {
    web_app_policy_manager_ = std::make_unique<WebAppPolicyManager>(
        profile, pending_app_manager_.get());
  }

  system_web_app_manager_ = std::make_unique<SystemWebAppManager>(
      profile, pending_app_manager_.get());

  notification_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::Source<Profile>(profile));

  web_app::ScanForExternalWebApps(
      profile, base::BindOnce(&WebAppProvider::OnScanForExternalWebApps,
                              weak_ptr_factory_.GetWeakPtr()));
}

// static
void WebAppProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ExtensionIdsMap::RegisterProfilePrefs(registry);
  WebAppPolicyManager::RegisterProfilePrefs(registry);
}

// static
bool WebAppProvider::CanInstallWebApp(
    const content::WebContents* web_contents) {
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider || !provider->install_manager_)
    return false;
  return provider->install_manager_->CanInstallWebApp(web_contents);
}

// static
void WebAppProvider::InstallWebApp(content::WebContents* web_contents,
                                   bool force_shortcut_app) {
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  if (!provider || !provider->install_manager_)
    return;
  provider->install_manager_->InstallWebApp(web_contents, force_shortcut_app,
                                            base::DoNothing());
}

void WebAppProvider::Reset() {
  // TODO(loyso): Make it independent to the order of destruction via using two
  // end-to-end passes:
  // 1) Do Reset() for each subsystem to nullify pointers (detach subsystems).
  // 2) Destroy subsystems.

  // PendingAppManager is used by WebAppPolicyManager and therefore should be
  // deleted after it.
  web_app_policy_manager_.reset();
  system_web_app_manager_.reset();
  pending_app_manager_.reset();

  install_manager_.reset();
  registrar_.reset();
}

void WebAppProvider::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& detals) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);

  // KeyedService::Shutdown() gets called when the profile is being destroyed,
  // but after DCHECK'ing that no RenderProcessHosts are being leaked. The
  // "chrome::NOTIFICATION_PROFILE_DESTROYED" notification gets sent before the
  // DCHECK so we use that to clean up RenderProcessHosts instead.
  Reset();
}

void WebAppProvider::OnScanForExternalWebApps(
    std::vector<web_app::PendingAppManager::AppInfo> app_infos) {
  pending_app_manager_->SynchronizeInstalledApps(
      std::move(app_infos), InstallSource::kExternalDefault);
}

}  // namespace web_app
