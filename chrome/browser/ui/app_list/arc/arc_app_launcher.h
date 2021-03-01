// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_LAUNCHER_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_LAUNCHER_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "components/arc/metrics/arc_metrics_constants.h"

namespace content {
class BrowserContext;
}

// Helper class for deferred ARC app launching in case app is not ready on the
// moment of request.
class ArcAppLauncher : public ArcAppListPrefs::Observer {
 public:
  ArcAppLauncher(content::BrowserContext* context,
                 const std::string& app_id,
                 const base::Optional<std::string>& launch_intent,
                 bool deferred_launch_allowed,
                 int64_t display_id,
                 arc::UserInteractionType interaction);
  ~ArcAppLauncher() override;

  bool app_launched() const { return app_launched_; }

  // ArcAppListPrefs::Observer:
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;

 private:
  bool MaybeLaunchApp(const std::string& app_id,
                      const ArcAppListPrefs::AppInfo& app_info);

  // Unowned pointer.
  content::BrowserContext* context_;
  // ARC app id.
  const std::string app_id_;
  // Optional intent to launch the app. If not set then app is started default
  // way.
  const base::Optional<std::string> launch_intent_;
  // If it is set to true that means app is allowed to launch in deferred mode
  // once it is registered, regardless it is ready or not. Otherwise app is
  // launched when it becomes ready.
  const bool deferred_launch_allowed_;
  // Display where the app should be launched.
  const int64_t display_id_;
  // Flag indicating that ARC app was launched.
  bool app_launched_ = false;
  // Enum that indicates what type of metric to record to UMA on launch.
  arc::UserInteractionType interaction_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppLauncher);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_LAUNCHER_H_
