// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_REGISTRATION_H_
#define COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_REGISTRATION_H_

#include "base/check.h"
#include "base/memory/raw_ref.h"
#include "base/types/pass_key.h"
#include "components/origin_gating/core/checker_id.h"

namespace origin_gating {

class OriginGatingService;

// Helper that unregisters an OriginGatingChecker with the OriginGatingService
// upon destruction.
//
// Both `service` and `id` must be non-null when constructed.
class OriginGatingRegistration {
 public:
  OriginGatingRegistration(base::PassKey<OriginGatingService>,
                           OriginGatingService& service,
                           CheckerId id);
  OriginGatingRegistration(const OriginGatingRegistration&) = delete;
  OriginGatingRegistration& operator=(const OriginGatingRegistration&) = delete;
  OriginGatingRegistration(OriginGatingRegistration&&) = delete;
  OriginGatingRegistration& operator=(OriginGatingRegistration&&) = delete;
  ~OriginGatingRegistration();

  CheckerId id() const {
    CHECK(!id_.is_null());
    return id_;
  }
  OriginGatingService& service() const { return *service_; }

 private:
  const raw_ref<OriginGatingService> service_;
  const CheckerId id_;
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_ORIGIN_GATING_REGISTRATION_H_
