// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_launcher.h"

#include <memory>

#include "ui/events/event_constants.h"

ArcAppLauncher::ArcAppLauncher(content::BrowserContext* context,
                               const std::string& app_id,
                               const base::Optional<std::string>& launch_intent,
                               bool deferred_launch_allowed,
                               int64_t display_id,
                               arc::UserInteractionType interaction)
    : context_(context),
      app_id_(app_id),
      launch_intent_(launch_intent),
      deferred_launch_allowed_(deferred_launch_allowed),
      display_id_(display_id),
      interaction_(interaction) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id_);
  if (!app_info || !MaybeLaunchApp(app_id, *app_info))
    prefs->AddObserver(this);
}

ArcAppLauncher::~ArcAppLauncher() {
  if (!app_launched_) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
    if (prefs)
      prefs->RemoveObserver(this);
    VLOG(2) << "App " << app_id_ << "was not launched.";
  }
}

void ArcAppLauncher::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  MaybeLaunchApp(app_id, app_info);
}

void ArcAppLauncher::OnAppStatesChanged(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  MaybeLaunchApp(app_id, app_info);
}

bool ArcAppLauncher::MaybeLaunchApp(const std::string& app_id,
                                    const ArcAppListPrefs::AppInfo& app_info) {
  DCHECK(!app_launched_);

  if (app_id != app_id_ || (!app_info.ready && !deferred_launch_allowed_) ||
      app_info.suspended) {
    return false;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs && prefs->GetApp(app_id_));
  prefs->RemoveObserver(this);

  if (!arc::LaunchAppWithIntent(context_, app_id_, launch_intent_, ui::EF_NONE,
                                interaction_, display_id_)) {
    VLOG(2) << "Failed to launch app: " + app_id_ + ".";
  }

  app_launched_ = true;
  return true;
}
