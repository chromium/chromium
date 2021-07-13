// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desk_template_app_launch_handler.h"

#include <string>

#include "base/notreached.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/full_restore/restore_data.h"
#include "extensions/common/constants.h"

DeskTemplateAppLaunchHandler::DeskTemplateAppLaunchHandler(Profile* profile)
    : chromeos::AppLaunchHandler(profile) {}

DeskTemplateAppLaunchHandler::~DeskTemplateAppLaunchHandler() = default;

void DeskTemplateAppLaunchHandler::SetRestoreDataAndLaunch(
    std::unique_ptr<full_restore::RestoreData> restore_data) {
  // TODO(sammiequon) : Investigate if we can early return if a launch is
  // currently underway.
  restore_data_ = std::move(restore_data);
  if (HasRestoreData()) {
    LaunchApps();
    LaunchBrowsers();
  }
}

base::WeakPtr<chromeos::AppLaunchHandler>
DeskTemplateAppLaunchHandler::GetWeakPtrAppLaunchHandler() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DeskTemplateAppLaunchHandler::LaunchBrowsers() {
  DCHECK(restore_data_);

  const auto& launch_list = restore_data_->app_id_to_launch_list();
  for (const auto& iter : launch_list) {
    if (iter.first != extension_misc::kChromeAppId)
      continue;

    for (const auto& window_iter : iter.second) {
      absl::optional<std::vector<GURL>> urls = window_iter.second->urls;
      if (!urls || urls->empty())
        continue;

      Browser::CreateParams create_params = Browser::CreateParams(
          Browser::TYPE_NORMAL, profile_, /*user_gesture=*/false);
      create_params.restore_id = window_iter.first;
      Browser* browser = Browser::Create(create_params);

      absl::optional<int32_t> active_tab_index =
          window_iter.second->active_tab_index;
      for (int i = 0; i < urls->size(); i++) {
        chrome::AddTabAt(
            browser, urls->at(i), /*index=*/-1,
            /*foreground=*/(active_tab_index && i == *active_tab_index));
      }

      browser->window()->ShowInactive();
    }
  }
}

void DeskTemplateAppLaunchHandler::RecordRestoredAppLaunch(
    apps::AppTypeName app_type_name) {
  // TODO: Add UMA Histogram.
  NOTIMPLEMENTED();
}
