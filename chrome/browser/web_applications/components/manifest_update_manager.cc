// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/manifest_update_manager.h"

#include "base/metrics/histogram_macros.h"
#include "base/util/values/values_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace web_app {

namespace {

const char kLastUpdateCheckKey[] = "last_update_check";

class AppPrefs {
 public:
  AppPrefs(Profile* profile, const GURL& origin) {
    settings_ = HostContentSettingsMapFactory::GetForProfile(profile);
    if (!settings_)
      return;
    origin_data_ = settings_->GetWebsiteSetting(
        origin, GURL(), ContentSettingsType::INSTALLED_WEB_APP_METADATA,
        std::string(), nullptr);
  }

  bool IsAvailable() const { return settings_; }

  const base::Value* GetAppData(const AppId& app_id) const {
    if (!origin_data_)
      return nullptr;
    return origin_data_->FindKey(app_id);
  }

  base::Value& GetAppDataMutable(const AppId& app_id) {
    DCHECK(IsAvailable());
    if (!origin_data_)
      origin_data_ =
          std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
    base::Value* app_data = origin_data_->FindKey(app_id);
    if (!app_data) {
      app_data = origin_data_->SetKey(
          app_id, base::Value(base::Value::Type::DICTIONARY));
    }
    return *app_data;
  }

  void Save(const GURL& origin) {
    DCHECK(IsAvailable());
    settings_->SetWebsiteSettingDefaultScope(
        origin, GURL(), ContentSettingsType::INSTALLED_WEB_APP_METADATA,
        std::string(), std::move(origin_data_));
  }

 private:
  HostContentSettingsMap* settings_ = nullptr;
  std::unique_ptr<base::Value> origin_data_;
};

}  // namespace

ManifestUpdateManager::ManifestUpdateManager(Profile* profile)
    : profile_(profile) {}

ManifestUpdateManager::~ManifestUpdateManager() = default;

void ManifestUpdateManager::SetSubsystems(AppRegistrar* registrar,
                                          WebAppUiManager* ui_manager,
                                          InstallManager* install_manager) {
  registrar_ = registrar;
  ui_manager_ = ui_manager;
  install_manager_ = install_manager;
}

void ManifestUpdateManager::Start() {
  registrar_observer_.Add(registrar_);
}

void ManifestUpdateManager::Shutdown() {
  registrar_observer_.RemoveAll();
}

void ManifestUpdateManager::MaybeUpdate(const GURL& url,
                                        const AppId& app_id,
                                        content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsLocalUpdating))
    return;

  if (app_id.empty() || !registrar_->IsLocallyInstalled(app_id)) {
    NotifyResult(url, ManifestUpdateResult::kNoAppInScope);
    return;
  }

  if (registrar_->IsPlaceholderApp(app_id)) {
    NotifyResult(url, ManifestUpdateResult::kAppIsPlaceholder);
    return;
  }

  if (base::Contains(tasks_, app_id))
    return;

  if (!MaybeConsumeUpdateCheck(url.GetOrigin(), app_id)) {
    NotifyResult(url, ManifestUpdateResult::kThrottled);
    return;
  }

  tasks_.insert_or_assign(
      app_id, std::make_unique<ManifestUpdateTask>(
                  url, app_id, web_contents,
                  base::Bind(&ManifestUpdateManager::OnUpdateStopped,
                             base::Unretained(this)),
                  hang_update_checks_for_testing_, *registrar_, ui_manager_,
                  install_manager_));
}

// AppRegistrarObserver:
void ManifestUpdateManager::OnWebAppUninstalled(const AppId& app_id) {
  auto it = tasks_.find(app_id);
  if (it != tasks_.end()) {
    NotifyResult(it->second->url(), ManifestUpdateResult::kAppUninstalled);
    tasks_.erase(it);
  }
  DCHECK(!tasks_.contains(app_id));
}

bool ManifestUpdateManager::MaybeConsumeUpdateCheck(const GURL& origin,
                                                    const AppId& app_id) {
  base::Optional<base::Time> last_check_time =
      GetLastUpdateCheckTime(origin, app_id);
  if (!last_check_time)
    return false;

  base::Time now = time_override_for_testing_.value_or(base::Time::Now());
  // Throttling updates to at most once per day is consistent with Android.
  // See |UPDATE_INTERVAL| in WebappDataStorage.java.
  constexpr base::TimeDelta kDelayBetweenChecks = base::TimeDelta::FromDays(1);
  if (now < *last_check_time + kDelayBetweenChecks)
    return false;

  SetLastUpdateCheckTime(origin, app_id, now);
  return true;
}

base::Optional<base::Time> ManifestUpdateManager::GetLastUpdateCheckTime(
    const GURL& origin,
    const AppId& app_id) const {
  AppPrefs app_prefs(profile_, origin);
  if (!app_prefs.IsAvailable())
    return base::nullopt;
  const base::Value* app_data = app_prefs.GetAppData(app_id);
  if (!app_data)
    return base::Time();
  return util::ValueToTime(app_data->FindKey(kLastUpdateCheckKey))
      .value_or(base::Time());
}

void ManifestUpdateManager::SetLastUpdateCheckTime(const GURL& origin,
                                                   const AppId& app_id,
                                                   base::Time time) {
  AppPrefs app_prefs(profile_, origin);
  if (!app_prefs.IsAvailable())
    return;
  base::Value& app_data = app_prefs.GetAppDataMutable(app_id);
  app_data.SetKey(kLastUpdateCheckKey, util::TimeToValue(time));
  app_prefs.Save(origin);
}

void ManifestUpdateManager::OnUpdateStopped(const ManifestUpdateTask& task,
                                            ManifestUpdateResult result) {
  DCHECK_EQ(&task, tasks_[task.app_id()].get());
  NotifyResult(task.url(), result);
  tasks_.erase(task.app_id());
}

void ManifestUpdateManager::SetResultCallbackForTesting(
    ResultCallback callback) {
  DCHECK(result_callback_for_testing_.is_null());
  result_callback_for_testing_ = std::move(callback);
}

void ManifestUpdateManager::NotifyResult(const GURL& url,
                                         ManifestUpdateResult result) {
  // Don't log kNoAppInScope because it will be far too noisy (most page loads
  // will hit it).
  if (result != ManifestUpdateResult::kNoAppInScope) {
    UMA_HISTOGRAM_ENUMERATION("Webapp.Update.ManifestUpdateResult", result);
  }
  if (result_callback_for_testing_)
    std::move(result_callback_for_testing_).Run(url, result);
}

}  // namespace web_app
