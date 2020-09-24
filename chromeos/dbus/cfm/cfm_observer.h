// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CFM_CFM_OBSERVER_H_
#define CHROMEOS_DBUS_CFM_CFM_OBSERVER_H_

#include <string>

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace chromeos {
namespace cfm {

class CfmObserver : public base::CheckedObserver {
 public:
  ~CfmObserver() override = default;

  // Called when Hotline is requesting a service to be connected.
  // Returns |true| if |service_id| matches mojom interface name
  virtual bool ServiceRequestReceived(const std::string& service_id) = 0;

 protected:
  CfmObserver() = default;
};

using CfmObserverList = base::ObserverList<CfmObserver>;

}  // namespace cfm
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CFM_CFM_OBSERVER_H_
