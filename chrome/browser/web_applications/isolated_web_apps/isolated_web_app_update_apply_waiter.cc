// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_apply_waiter.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"

namespace web_app {

IsolatedWebAppUpdateApplyWaiter::IsolatedWebAppUpdateApplyWaiter(
    IsolatedWebAppUrlInfo url_info,
    WebAppUiManager& ui_manager)
    : url_info_(std::move(url_info)), ui_manager_(ui_manager) {}

IsolatedWebAppUpdateApplyWaiter::~IsolatedWebAppUpdateApplyWaiter() = default;

void IsolatedWebAppUpdateApplyWaiter::Wait(Profile* profile,
                                           Callback callback) {
  CHECK(callback_.is_null());
  callback_ = std::move(callback);
  keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::ISOLATED_WEB_APP_UPDATE,
      KeepAliveRestartOption::DISABLED);
  // Off the record profiles cannot have `ScopedProfileKeepAlive`s.
  profile_keep_alive_ =
      profile->IsOffTheRecord()
          ? nullptr
          : std::make_unique<ScopedProfileKeepAlive>(
                profile, ProfileKeepAliveOrigin::kIsolatedWebAppUpdate);

  // If we do not need to wait, then synchronously signal so that an update
  // apply task can immediately be scheduled before another update discovery
  // task is run.
  if (ui_manager_->GetNumWindowsForApp(url_info_.app_id()) == 0) {
    Signal();
    return;
  }

  ui_manager_->NotifyOnAllAppWindowsClosed(
      url_info_.app_id(),
      // Do _not_ bind the keep alives to this callback - we want the
      // lifetimes of the keep alives to be tied to `this`, and not
      // `ui_manager_` (which may keep the callback around for longer).
      base::BindOnce(&IsolatedWebAppUpdateApplyWaiter::Signal,
                     weak_factory_.GetWeakPtr()));
}

base::Value IsolatedWebAppUpdateApplyWaiter::AsDebugValue() const {
  return base::Value(
      base::Value::Dict()
          .Set("app_id", url_info_.app_id())
          .Set("bundle_id", url_info_.web_bundle_id().id())
          .Set("signalled", callback_.is_null())
          .Set("num_app_windows",
               base::saturated_cast<int>(
                   ui_manager_->GetNumWindowsForApp(url_info_.app_id()))));
}

void IsolatedWebAppUpdateApplyWaiter::Signal() {
  std::move(callback_).Run(std::move(keep_alive_),
                           std::move(profile_keep_alive_));
}

}  // namespace web_app
