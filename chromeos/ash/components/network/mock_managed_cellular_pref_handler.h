// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_MOCK_MANAGED_CELLULAR_PREF_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_MOCK_MANAGED_CELLULAR_PREF_HANDLER_H_

#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class COMPONENT_EXPORT(CHROMEOS_NETWORK) MockManagedCellularPrefHandler
    : public ManagedCellularPrefHandler {
 public:
  MockManagedCellularPrefHandler();
  MockManagedCellularPrefHandler(const MockManagedCellularPrefHandler&) =
      delete;
  MockManagedCellularPrefHandler& operator=(
      const MockManagedCellularPrefHandler&) = delete;
  ~MockManagedCellularPrefHandler() override;

  // ManagedCellularPrefHandler overrides
  MOCK_METHOD1(AddApnMigratedIccid, void(const std::string&));
  MOCK_CONST_METHOD1(ContainsApnMigratedIccid, bool(const std::string&));
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_MOCK_MANAGED_CELLULAR_PREF_HANDLER_H_
