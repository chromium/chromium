// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_FACTOR_AUTH_VIEW_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_FACTOR_AUTH_VIEW_FACTORY_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/auth_panel/factor_auth_view.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

// This class is responsible for creating the different `FactorAuthView` objects
// that will be owned and managed by `AuthPanel`. This is done mainly to
// facilitate dependency injection and testing and to centralize the creation
// logic into one class, freeing up `AuthPanel` to deal exclusively with UI
// update logic.
class FactorAuthViewFactory {
 public:
  using FactorAuthViewCreator =
      base::RepeatingCallback<std::unique_ptr<FactorAuthView>()>;

  FactorAuthViewFactory();
  FactorAuthViewFactory(const FactorAuthViewFactory&) = delete;
  FactorAuthViewFactory(FactorAuthViewFactory&&) = delete;
  FactorAuthViewFactory& operator=(const FactorAuthViewFactory&) = delete;
  FactorAuthViewFactory& operator=(FactorAuthViewFactory&&) = delete;
  ~FactorAuthViewFactory() = default;

  // Create the respective `FactorAuthView` specified by the `AshAuthFactor`
  // enum.
  std::unique_ptr<FactorAuthView> CreateFactorAuthView(AshAuthFactor factor);

 private:
  // `FactorAuthView` factory functions:
  std::unique_ptr<FactorAuthView> CreatePasswordView();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_FACTOR_AUTH_VIEW_FACTORY_H_
