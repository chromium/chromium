// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_FACTOR_AUTH_VIEW_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_FACTOR_AUTH_VIEW_H_

#include "chromeos/ash/components/auth_panel/impl/auth_factor_store.h"
#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// Interface common to all factor views, abstracts an auth factor view on the
// login, lock screen, or in-session.
class FactorAuthView : public views::View {
  METADATA_HEADER(FactorAuthView, views::View)

 public:
  ~FactorAuthView() override = default;

  virtual void OnStateChanged(const AuthFactorStore::State& state) = 0;

  // Returns the respective `AshAuthFactor` handled by this view.
  virtual AshAuthFactor GetFactor() = 0;
};

}  // namespace ash

#endif  //  CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_FACTOR_AUTH_VIEW_H_
