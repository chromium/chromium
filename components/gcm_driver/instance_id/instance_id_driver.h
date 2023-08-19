// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_DRIVER_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_DRIVER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace instance_id {

class InstanceID;

// Bridge between Instance ID users in Chrome and the platform-specific
// implementation.
//
// Create instances of this class with |InstanceIDProfileServiceFactory|.
class InstanceIDDriver {
 public:
  explicit InstanceIDDriver(gcm::GCMDriver* gcm_driver);

  InstanceIDDriver(const InstanceIDDriver&) = delete;
  InstanceIDDriver& operator=(const InstanceIDDriver&) = delete;

  virtual ~InstanceIDDriver();

  // Returns the InstanceID that provides the Instance ID service for the given
  // application. The lifetime of the InstanceID will be managed by this class.
  // App IDs are arbitrary strings that typically look like "chrome.foo.bar".
  virtual InstanceID* GetInstanceID(const std::string& app_id);

  // Removes the InstanceID when it is not longer needed, i.e. the app is being
  // uninstalled.
  virtual void RemoveInstanceID(const std::string& app_id);

  // Returns true if the InstanceID for the given application has been created.
  // This is currently only used for testing purpose.
  virtual bool ExistsInstanceID(const std::string& app_id) const;

 private:
  // Owned by GCMProfileServiceFactory, which is a dependency of
  // InstanceIDProfileServiceFactory, which owns this.
  raw_ptr<gcm::GCMDriver, AcrossTasksDanglingUntriaged> gcm_driver_;

  std::map<std::string, std::unique_ptr<InstanceID>> instance_id_map_;
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_DRIVER_H_
