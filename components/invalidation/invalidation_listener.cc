// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/invalidation_listener.h"

#include <memory>

#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/invalidation_listener_impl.h"

namespace invalidation {

std::string DirectInvalidation::type() const {
  return invalidation::Invalidation::topic();
}

base::Time DirectInvalidation::issue_timestamp() const {
  return base::Time::UnixEpoch() + base::Microseconds(version());
}

// static
std::unique_ptr<InvalidationListener> InvalidationListener::Create(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    std::string project_number,
    std::string log_prefix) {
  return std::make_unique<InvalidationListenerImpl>(
      gcm_driver, instance_id_driver, std::move(project_number),
      std::move(log_prefix));
}

}  // namespace invalidation
