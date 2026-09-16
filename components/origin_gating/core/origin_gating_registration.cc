// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_registration.h"

#include "base/check.h"
#include "components/origin_gating/core/origin_gating_service.h"

namespace origin_gating {

OriginGatingRegistration::OriginGatingRegistration(
    base::PassKey<OriginGatingService>,
    OriginGatingService& service,
    CheckerId id)
    : service_(service), id_(id) {
  CHECK(!id_.is_null());
}

OriginGatingRegistration::~OriginGatingRegistration() {
  service_->UnregisterChecker(base::PassKey<OriginGatingRegistration>(), id_);
}

}  // namespace origin_gating
