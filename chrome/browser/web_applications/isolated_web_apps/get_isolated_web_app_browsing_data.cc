// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/get_isolated_web_app_browsing_data.h"

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/get_isolated_web_app_browsing_data_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "url/origin.h"

namespace web_app {

void GetIsolatedWebAppBrowsingData(
    Profile* profile,
    base::OnceCallback<void(base::flat_map<url::Origin, uint64_t>)> callback,
    const base::Location& call_location) {
  auto* provider = WebAppProvider::GetForWebApps(profile);
  if (!provider) {
    std::move(callback).Run({});
    return;
  }
  provider->command_manager().ScheduleCommand(
      std::make_unique<GetIsolatedWebAppBrowsingDataCommand>(
          profile, std::move(callback)),
      call_location);
}

}  // namespace web_app
