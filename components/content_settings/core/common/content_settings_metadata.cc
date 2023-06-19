// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_metadata.h"

#include <stddef.h>

#include <tuple>

#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings_constraints.h"

namespace content_settings {

RuleMetaData::RuleMetaData() = default;

void RuleMetaData::SetFromConstraints(
    const ContentSettingConstraints& constraints) {
  session_model_ = constraints.session_model();
  SetExpirationAndLifetime(constraints.expiration(), constraints.lifetime());
}

void RuleMetaData::SetExpirationAndLifetime(base::Time expiration,
                                            base::TimeDelta lifetime) {
  CHECK_EQ(lifetime.is_zero(), expiration.is_null());
  CHECK_GE(lifetime, base::TimeDelta());
  expiration_ = expiration;
  lifetime_ = lifetime;
}

bool RuleMetaData::operator==(const RuleMetaData& other) const {
  return std::tie(last_modified_, last_visited_, expiration_, session_model_,
                  lifetime_) == std::tie(other.last_modified_,
                                         other.last_visited_, other.expiration_,
                                         other.session_model_, other.lifetime_);
}

// static
base::TimeDelta RuleMetaData::ComputeLifetime(base::TimeDelta lifetime,
                                              base::Time expiration) {
  // The stored metadata may have included an expiration without a lifetime; but
  // if it included a lifetime, it must also have included an expiration.
  CHECK(lifetime.is_zero() || !expiration.is_null());

  if (expiration.is_null()) {
    return base::TimeDelta();
  }
  if (!lifetime.is_zero()) {
    return lifetime;
  }

  lifetime = expiration - base::Time::Now();
  if (lifetime < base::TimeDelta()) {
    // Not setting to zero, since a non-null expiration doesn't make sense with
    // a zero lifetime.
    lifetime = base::Microseconds(1);
  }
  return lifetime;
}

}  // namespace content_settings
