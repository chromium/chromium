// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_H_

#include <memory>

#include "base/macros.h"
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

  ~InstanceIDProfileService() override;

  InstanceIDDriver* driver() const { return driver_.get(); }

 private:
  std::unique_ptr<InstanceIDDriver> driver_;

  DISALLOW_COPY_AND_ASSIGN(InstanceIDProfileService);
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_H_
