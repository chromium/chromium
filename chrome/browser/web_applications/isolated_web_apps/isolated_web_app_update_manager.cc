// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/isolated_web_apps_policy.h"

namespace web_app {

IsolatedWebAppUpdateManager::IsolatedWebAppUpdateManager(Profile& profile)
    : profile_(profile),
      automatic_updates_enabled_(
          content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(&profile) &&
          base::FeatureList::IsEnabled(
              features::kIsolatedWebAppAutomaticUpdates)) {}

IsolatedWebAppUpdateManager::~IsolatedWebAppUpdateManager() = default;

void IsolatedWebAppUpdateManager::SetProvider(base::PassKey<WebAppProvider>,
                                              WebAppProvider& provider) {
  provider_ = &provider;
}

void IsolatedWebAppUpdateManager::Start() {
  has_started_ = true;
  if (!automatic_updates_enabled_) {
    return;
  }

  // TODO(cmfcmf): Implement this.
}

void IsolatedWebAppUpdateManager::Shutdown() {}

base::Value IsolatedWebAppUpdateManager::AsDebugValue() const {
  return base::Value(base::Value::Dict().Set("automatic_updates_enabled",
                                             automatic_updates_enabled_));
}

void IsolatedWebAppUpdateManager::SetEnableAutomaticUpdatesForTesting(
    bool automatic_updates_enabled) {
  CHECK(!has_started_);
  automatic_updates_enabled_ = automatic_updates_enabled;
}

}  // namespace web_app
