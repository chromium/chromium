// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_SETTINGS_MOCK_PLUS_ADDRESS_SETTING_SERVICE_H_
#define COMPONENTS_PLUS_ADDRESSES_SETTINGS_MOCK_PLUS_ADDRESS_SETTING_SERVICE_H_

#include "components/plus_addresses/settings/plus_address_setting_service.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace plus_addresses {

class MockPlusAddressSettingService : public PlusAddressSettingService {
 public:
  MockPlusAddressSettingService();
  ~MockPlusAddressSettingService() override;

  MOCK_METHOD(bool, GetIsPlusAddressesEnabled, (), (const, override));
  MOCK_METHOD(bool, GetHasAcceptedNotice, (), (const override));
  MOCK_METHOD(void, SetHasAcceptedNotice, (), (override));

  MOCK_METHOD(std::unique_ptr<syncer::DataTypeControllerDelegate>,
              GetSyncControllerDelegate,
              (),
              (override));
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_SETTINGS_MOCK_PLUS_ADDRESS_SETTING_SERVICE_H_
