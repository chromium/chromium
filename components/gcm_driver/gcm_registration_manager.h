// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_REGISTRATION_MANAGER_H_
#define COMPONENTS_GCM_DRIVER_GCM_REGISTRATION_MANAGER_H_

#include <string>

namespace gcm {

// Manages GCM app registrations and Instance ID data on the IO thread.
class GCMRegistrationManager {
 public:
  virtual ~GCMRegistrationManager() = default;

  // Adds the Instance ID data for a particular app to the cache and persistent
  // store.
  virtual void AddInstanceIDData(const std::string& app_id,
                                 const std::string& instance_id,
                                 const std::string& extra_data) = 0;
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_REGISTRATION_MANAGER_H_
