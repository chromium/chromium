// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"

namespace gcm {
class GCMDriver;
}

namespace instance_id {

class InstanceIDDriver;

// Providing Instance ID support, via InstanceIDDriver, to a profile.
class InstanceIDProfileService : public KeyedService {
 public:
  InstanceIDProfileService(gcm::GCMDriver* driver, bool is_off_the_record);

  InstanceIDProfileService(const InstanceIDProfileService&) = delete;
  InstanceIDProfileService& operator=(const InstanceIDProfileService&) = delete;

  ~InstanceIDProfileService() override;

  InstanceIDDriver* driver() const { return driver_.get(); }

  static std::unique_ptr<InstanceIDProfileService> CreateForTests(
      std::unique_ptr<InstanceIDDriver> instance_id_driver);

 private:
  // Private constructor used for tests only.
  explicit InstanceIDProfileService(
      std::unique_ptr<InstanceIDDriver> instance_id_driver);

  std::unique_ptr<InstanceIDDriver> driver_;
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_H_
