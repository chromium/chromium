// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CORE_DEPENDENCY_FACTORY_H_
#define COMPONENTS_ENTERPRISE_CORE_DEPENDENCY_FACTORY_H_

namespace policy {
class CloudPolicyManager;
}  // namespace policy

namespace enterprise_core {

// Factory that can be used to lazy-load dependencies.
class DependencyFactory {
 public:
  virtual ~DependencyFactory() = default;

  virtual policy::CloudPolicyManager* GetUserCloudPolicyManager() const = 0;
};

}  // namespace enterprise_core

#endif  // COMPONENTS_ENTERPRISE_CORE_DEPENDENCY_FACTORY_H_
