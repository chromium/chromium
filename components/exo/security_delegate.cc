// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/security_delegate.h"

#include <memory>
#include <string>

#include "components/exo/shell_surface_util.h"

namespace exo {

namespace {

class DefaultSecurityDelegate : public SecurityDelegate {
 public:
  ~DefaultSecurityDelegate() override = default;

  std::string GetSecurityContext() const override { return ""; }
};

}  // namespace

SecurityDelegate::~SecurityDelegate() = default;

// static
std::unique_ptr<SecurityDelegate>
SecurityDelegate::GetDefaultSecurityDelegate() {
  return std::make_unique<DefaultSecurityDelegate>();
}

bool SecurityDelegate::CanSelfActivate(aura::Window* window) const {
  // TODO(b/233691818): The default should be "false", and clients should
  // override that if they need to self-activate.
  //
  // Unfortunately, several clients don't have their own SecurityDelegate yet,
  // so we will continue to use the old exo::Permissions stuff until they do.
  return HasPermissionToActivate(window);
}

}  // namespace exo
