// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_STATUS_OBSERVER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_STATUS_OBSERVER_H_

#include "base/observer_list_types.h"

namespace ip_protection {

class IpProtectionStatusObserver : public base::CheckedObserver {
 public:
  ~IpProtectionStatusObserver() override = default;
  virtual void OnFirstSubresourceProxiedOnCurrentPrimaryPage() const = 0;
};
}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_STATUS_OBSERVER_H_
