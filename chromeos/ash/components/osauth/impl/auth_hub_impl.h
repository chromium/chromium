// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_IMPL_H_

#include <memory>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_mode_lifecycle.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/auth_hub.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthHubImpl
    : public AuthHub,
      public AuthHubModeLifecycle::Owner {
 public:
  explicit AuthHubImpl();
  ~AuthHubImpl() override;

  // ----- AuthHub implementation:

  // High-level lifecycle:
  void InitializeForMode(AuthHubMode target) override;
  void EnsureInitialized(base::OnceClosure on_initialized) override;

  // ----- AuthHubModeLifecycle implementation:
  void OnReadyForMode(
      AuthHubMode mode,
      AuthHubModeLifecycle::EnginesMap available_engines) override;
  void OnExitedMode(AuthHubMode mode) override;
  void OnModeShutdown() override;

 private:
  base::OnceCallbackList<void()> on_initialized_listeners_;
  std::unique_ptr<AuthHubModeLifecycle> mode_lifecycle_;
  base::WeakPtrFactory<AuthHubImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_IMPL_H_
