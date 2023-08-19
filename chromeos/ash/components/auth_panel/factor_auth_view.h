// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_FACTOR_AUTH_VIEW_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_FACTOR_AUTH_VIEW_H_

#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "ui/views/view.h"

namespace ash {

// Interface common to all factor views, abstracts an auth factor view on the
// login, lock screen, or in-session.
class FactorAuthView : public views::View {
 public:
  ~FactorAuthView() override;

  // Called from `AuthPanel` when the state of an auth factor changes, this
  // would normally, but not always, imply visual changes that would be
  // displayed by the respective view.
  virtual void OnFactorStateChanged(AuthFactorState auth_factor_state) = 0;

  // Called from `AuthPanel` when an authentication attempt was not successful
  // using a particular auth factor, this would imply visual changes such as
  // error messages, color changes, etc.
  virtual void OnAuthFailure() = 0;

  // Called from `AuthPanel` when an authentication attempt was successful using
  // a particular auth factor, this would imply visual changes such as
  // checkmarks, color changes, etc.
  virtual void OnAuthSuccess() = 0;

  // Returns the respective `AshAuthFactor` handled by this view.
  virtual AshAuthFactor GetFactor() = 0;
};

}  // namespace ash

#endif  //  CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_FACTOR_AUTH_VIEW_H_
