// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_surface_registry.h"

#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"
namespace ash {

AuthSurfaceRegistry::AuthSurfaceRegistry() = default;

AuthSurfaceRegistry::~AuthSurfaceRegistry() = default;

void AuthSurfaceRegistry::NotifyLoginScreenAuthDialogShown(
    AuthHubConnector* connector) {
  NOTIMPLEMENTED();
}

void AuthSurfaceRegistry::NotifyLockScreenAuthDialogShown(
    AuthHubConnector* connector) {
  NOTIMPLEMENTED();
}

void AuthSurfaceRegistry::NotifyInSessionAuthDialogShown(
    AuthHubConnector* connector) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](AuthSurfaceRegistry* self, AuthHubConnector* connector) {
            self->callback_list_.Notify(connector, AuthSurface::kInSession);
          },
          this, connector));
}

base::CallbackListSubscription AuthSurfaceRegistry::RegisterShownCallback(
    CallbackList::CallbackType on_shown) {
  return callback_list_.Add(std::move(on_shown));
}

}  // namespace ash
