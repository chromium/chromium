// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_LEGACY_AUTH_SURFACE_REGISTRY_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_LEGACY_AUTH_SURFACE_REGISTRY_H_

#include "base/callback_list.h"
#include "base/component_export.h"

namespace ash {

class AuthHubConnector;

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
    LegacyAuthSurfaceRegistry {
 public:
  enum class AuthSurface {
    kLoginScreen,
    kLockScreen,
    kInSession,
    kMaxValue = kInSession,
  };

  LegacyAuthSurfaceRegistry();
  ~LegacyAuthSurfaceRegistry();

  using CallbackList =
      base::OnceCallbackList<void(AuthHubConnector*, AuthSurface)>;

  void NotifyLoginScreenAuthDialogShown(AuthHubConnector* connector);
  void NotifyLockScreenAuthDialogShown(AuthHubConnector* connector);
  void NotifyInSessionAuthDialogShown(AuthHubConnector* connector);

  base::CallbackListSubscription RegisterShownCallback(
      CallbackList::CallbackType on_shown);

 private:
  base::OnceCallbackList<void(AuthHubConnector*, AuthSurface surface)>
      callback_list_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_LEGACY_AUTH_SURFACE_REGISTRY_H_
