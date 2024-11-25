// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_router_ui_service.h"

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/browser/ui/media_router/media_router_ui_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/user_prefs/user_prefs.h"

namespace media_router {

MediaRouterUIService::MediaRouterUIService(Profile* profile)
    : MediaRouterUIService(profile, nullptr) {}

MediaRouterUIService::MediaRouterUIService(
    Profile* profile,
    std::unique_ptr<CastToolbarButtonController> action_controller)
    : profile_(profile),
      action_controller_(std::move(action_controller)),
      profile_pref_registrar_(std::make_unique<PrefChangeRegistrar>()) {
  profile_pref_registrar_->Init(profile->GetPrefs());
  profile_pref_registrar_->Add(
      ::prefs::kEnableMediaRouter,
      base::BindRepeating(&MediaRouterUIService::ConfigureService,
                          base::Unretained(this)));
  ConfigureService();
}

MediaRouterUIService::~MediaRouterUIService() {}

void MediaRouterUIService::Shutdown() {
  DisableService();
}

// static
MediaRouterUIService* MediaRouterUIService::Get(Profile* profile) {
  return MediaRouterUIServiceFactory::GetForBrowserContext(profile);
}

CastToolbarButtonController* MediaRouterUIService::action_controller() {
  return action_controller_.get();
}

void MediaRouterUIService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MediaRouterUIService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MediaRouterUIService::ConfigureService() {
  if (MediaRouterEnabled(profile_)) {
    if (!action_controller_) {
      action_controller_ =
          std::make_unique<CastToolbarButtonController>(profile_);
    }
#if BUILDFLAG(IS_CHROMEOS)
    if (GlobalMediaControlsCastStartStopEnabled(profile_)) {
      // Ensure that MediaNotificationService is instantiated so that it can
      // show the Cast device picker in Global Media Controls.
      MediaNotificationServiceFactory::GetForProfile(profile_);
    }
#endif
  } else {
    DisableService();
  }
}

void MediaRouterUIService::DisableService() {
  for (auto& observer : observers_)
    observer.OnServiceDisabled();
  action_controller_.reset();
}

}  // namespace media_router
