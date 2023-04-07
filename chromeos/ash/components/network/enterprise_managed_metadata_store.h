// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_ENTERPRISE_MANAGED_METADATA_STORE_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_ENTERPRISE_MANAGED_METADATA_STORE_H_

#include "base/component_export.h"

namespace ash {

// Stores metadata about whether the device is enterprise managed or not.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) EnterpriseManagedMetadataStore {
 public:
  EnterpriseManagedMetadataStore();
  EnterpriseManagedMetadataStore(const EnterpriseManagedMetadataStore&) =
      delete;
  EnterpriseManagedMetadataStore& operator=(
      const EnterpriseManagedMetadataStore&) = delete;
  ~EnterpriseManagedMetadataStore();

  void set_is_enterprise_managed(bool is_enterprise_managed) {
    is_enterprise_managed_ = is_enterprise_managed;
  }
  bool is_enterprise_managed() const { return is_enterprise_managed_; }

 private:
  bool is_enterprise_managed_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_ENTERPRISE_MANAGED_METADATA_STORE_H_
