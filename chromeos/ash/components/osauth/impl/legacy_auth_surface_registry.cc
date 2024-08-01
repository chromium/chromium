// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/legacy_auth_surface_registry.h"

#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

LegacyAuthSurfaceRegistry::LegacyAuthSurfaceRegistry() = default;

LegacyAuthSurfaceRegistry::~LegacyAuthSurfaceRegistry() = default;

void LegacyAuthSurfaceRegistry::NotifyLoginScreenAuthDialogShown(
    AuthHubConnector* connector) {
  NOTIMPLEMENTED();
}

void LegacyAuthSurfaceRegistry::NotifyLockScreenAuthDialogShown(
    AuthHubConnector* connector) {
  NOTIMPLEMENTED();
}

void LegacyAuthSurfaceRegistry::NotifyInSessionAuthDialogShown(
    AuthHubConnector* connector) {
  callback_list_.Notify(connector, AuthSurface::kInSession);
}

base::CallbackListSubscription LegacyAuthSurfaceRegistry::RegisterShownCallback(
    CallbackList::CallbackType on_shown) {
  return callback_list_.Add(std::move(on_shown));
}

}  // namespace ash
