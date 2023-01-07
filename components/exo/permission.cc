// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/permission.h"

#include "base/time/time.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(exo::Permission*)

namespace exo {

// Permission object allowing this window to activate itself.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(exo::Permission, kPermissionKey, nullptr)

Permission::Permission(Permission::Capability capability)
    : capability_(capability), expiry_(base::Time::Max()) {}

Permission::Permission(Permission::Capability capability,
                       base::TimeDelta timeout)
    : capability_(capability), expiry_(base::Time::Now() + timeout) {}

Permission::~Permission() = default;

void Permission::Revoke() {
  // Revoke the permission by setting its expiry to be in the past.
  expiry_ = {};
}

bool Permission::Check(Permission::Capability capability) const {
  return capability_ == capability && base::Time::Now() < expiry_;
}

}  // namespace exo
