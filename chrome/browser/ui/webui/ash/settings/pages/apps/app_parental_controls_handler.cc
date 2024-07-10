// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/apps/app_parental_controls_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/app_controls_notifier.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/mojom/app_parental_controls_handler.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash::settings {

namespace {
constexpr size_t kValidPinLength = 6u;
const char kNumericOnlyRegex[] = R"(^\d+$)";

app_parental_controls::mojom::AppPtr CreateAppPtr(
    const apps::AppUpdate& update) {
  auto app = app_parental_controls::mojom::App::New();
  app->id = update.AppId();
  app->title = update.Name();
  app->is_blocked =
      update.Readiness() == apps::Readiness::kDisabledByLocalSettings;
  return app;
}

bool ShouldIncludeApp(const apps::AppUpdate& update) {
  // Only apps shown in the App Management page should be shown.
  return update.ShowInManagement().value_or(false) &&
         update.AppType() == apps::AppType::kArc;
}

app_parental_controls::mojom::PinValidationResult GetPinValidationResult(
    const std::string& pin) {
  if (pin.length() != kValidPinLength) {
    return app_parental_controls::mojom::PinValidationResult::kPinLengthError;
  }
  if (!RE2::FullMatch(pin, kNumericOnlyRegex)) {
    return app_parental_controls::mojom::PinValidationResult::kPinNumericError;
  }
  return app_parental_controls::mojom::PinValidationResult::
      kPinValidationSuccess;
}
}  // namespace

AppParentalControlsHandler::AppParentalControlsHandler(
    apps::AppServiceProxy* app_service_proxy,
    Profile* profile)
    : app_service_proxy_(app_service_proxy),
      app_controls_notifier_(
          std::make_unique<on_device_controls::AppControlsNotifier>(profile)),
      blocked_app_registry_(
          std::make_unique<on_device_controls::BlockedAppRegistry>(
              app_service_proxy,
              profile->GetPrefs())),
      profile_(profile) {
  CHECK(app_service_proxy_);
  CHECK(blocked_app_registry_);

  app_registry_cache_observer_.Observe(&app_service_proxy_->AppRegistryCache());
  app_controls_notifier_->MaybeShowAppControlsNotification();
}

AppParentalControlsHandler::~AppParentalControlsHandler() = default;

void AppParentalControlsHandler::BindInterface(
    mojo::PendingReceiver<
        app_parental_controls::mojom::AppParentalControlsHandler> receiver) {
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(std::move(receiver));
}

void AppParentalControlsHandler::GetApps(GetAppsCallback callback) {
  std::move(callback).Run(GetAppList());
}

void AppParentalControlsHandler::UpdateApp(const std::string& id,
                                           bool is_blocked) {
  if (is_blocked) {
    blocked_app_registry_->AddApp(id);
    return;
  }
  blocked_app_registry_->RemoveApp(id);
}

void AppParentalControlsHandler::AddObserver(
    mojo::PendingRemote<
        app_parental_controls::mojom::AppParentalControlsObserver> observer) {
  observer_list_.Add(std::move(observer));
}

void AppParentalControlsHandler::OnControlsDisabled() {
  profile_->GetPrefs()->SetString(prefs::kOnDeviceAppControlsPin,
                                  std::string());
  profile_->GetPrefs()->SetBoolean(prefs::kOnDeviceAppControlsSetupCompleted,
                                   false);
  blocked_app_registry_->RemoveAllApps();
}

void AppParentalControlsHandler::ValidatePin(const std::string& pin,
                                             ValidatePinCallback callback) {
  std::move(callback).Run(GetPinValidationResult(pin));
}

void AppParentalControlsHandler::SetUpPin(const std::string& pin,
                                          SetUpPinCallback callback) {
  if (GetPinValidationResult(pin) !=
      app_parental_controls::mojom::PinValidationResult::
          kPinValidationSuccess) {
    std::move(callback).Run(false);
    return;
  }
  profile_->GetPrefs()->SetString(prefs::kOnDeviceAppControlsPin, pin);
  profile_->GetPrefs()->SetBoolean(prefs::kOnDeviceAppControlsSetupCompleted,
                                   true);
  profile_->GetPrefs()->CommitPendingWrite();
  std::move(callback).Run(true);
}

void AppParentalControlsHandler::VerifyPin(const std::string& pin,
                                           VerifyPinCallback callback) {
  std::string stored_pin =
      profile_->GetPrefs()->GetString(prefs::kOnDeviceAppControlsPin);
  std::move(callback).Run(pin == stored_pin);
}

void AppParentalControlsHandler::IsSetupCompleted(
    IsSetupCompletedCallback callback) {
  std::move(callback).Run(profile_->GetPrefs()->GetBoolean(
      prefs::kOnDeviceAppControlsSetupCompleted));
}

void AppParentalControlsHandler::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.ReadinessChanged() && ShouldIncludeApp(update)) {
    if (!apps_util::IsInstalled(update.Readiness())) {
      NotifyAppRemoved(CreateAppPtr(update));
      return;
    }
    NotifyAppInstalledOrUpdated(CreateAppPtr(update));
  }
}

void AppParentalControlsHandler::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

std::vector<app_parental_controls::mojom::AppPtr>
AppParentalControlsHandler::GetAppList() {
  std::vector<app_parental_controls::mojom::AppPtr> apps;
  app_service_proxy_->AppRegistryCache().ForEachApp(
      [&apps](const apps::AppUpdate& update) {
        if (ShouldIncludeApp(update) &&
            apps_util::IsInstalled(update.Readiness())) {
          apps.push_back(CreateAppPtr(update));
        }
      });
  return apps;
}

void AppParentalControlsHandler::NotifyAppInstalledOrUpdated(
    app_parental_controls::mojom::AppPtr app) {
  for (const auto& observer : observer_list_) {
    observer->OnAppInstalledOrUpdated(app.Clone());
  }
}

void AppParentalControlsHandler::NotifyAppRemoved(
    app_parental_controls::mojom::AppPtr app) {
  for (const auto& observer : observer_list_) {
    observer->OnAppUninstalled(app.Clone());
  }
}

}  // namespace ash::settings
