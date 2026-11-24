// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_REGISTRATION_MANAGER_H_
#define COMPONENTS_GCM_DRIVER_GCM_REGISTRATION_MANAGER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/registration_info.h"

namespace gcm {

// Manages GCM app registrations and Instance ID data on the IO thread.
class GCMRegistrationManager {
 public:
  using GetTokenCallback = base::OnceCallback<void(const std::string& token,
                                                   GCMClient::Result result)>;

  virtual ~GCMRegistrationManager() = default;

  // Obtains an Instance ID token for `token_info`, invoking `callback`
  // synchronously (on a cache hit) or asynchronously.
  virtual void GetToken(scoped_refptr<InstanceIDTokenInfo> token_info,
                        GetTokenCallback callback) = 0;

  // Validates that cached registration info matches `registration_info` and
  // `registration_id`.
  virtual bool ValidateRegistration(
      scoped_refptr<RegistrationInfo> registration_info,
      const std::string& registration_id) = 0;

  // Returns the list of currently registered app IDs.
  virtual std::vector<std::string> GetRegisteredAppIds() const = 0;

  // Adds the Instance ID data for a particular app to the cache and persistent
  // store.
  virtual void AddInstanceIDData(const std::string& app_id,
                                 const std::string& instance_id,
                                 const std::string& extra_data) = 0;

  // Removes the Instance ID data for a particular app from the cache and
  // persistent store.
  virtual void RemoveInstanceIDData(const std::string& app_id) = 0;

  // Retrieves the Instance ID data (`{instance_id, extra_data}`) for `app_id`
  // from the cache, or a pair of empty strings if not found.
  virtual std::pair<std::string, std::string> GetInstanceIDData(
      const std::string& app_id) const = 0;
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_REGISTRATION_MANAGER_H_
