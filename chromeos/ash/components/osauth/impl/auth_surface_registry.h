// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_SURFACE_REGISTRY_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_SURFACE_REGISTRY_H_

#include "base/callback_list.h"
#include "base/component_export.h"

namespace ash {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthSurfaceRegistry {
 public:
  enum class AuthSurface {
    kLoginScreen,
    kLockScreen,
    kInSession,
    kMaxValue = kInSession,
  };

  AuthSurfaceRegistry();
  ~AuthSurfaceRegistry();

  using CallbackList = base::OnceCallbackList<void(AuthSurface)>;

  void NotifyLoginScreenAuthDialogShown();
  void NotifyLockScreenAuthDialogShown();
  void NotifyInSessionAuthDialogShown();

  base::CallbackListSubscription RegisterShownCallback(
      CallbackList::CallbackType on_shown);

 private:
  CallbackList callback_list_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_SURFACE_REGISTRY_H_
