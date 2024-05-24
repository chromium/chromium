// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_H_
#define COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace plus_addresses {

// Manages settings for `PlusAddressService`. These settings differ from regular
// prefs, since they originate from the user's account and are available beyond
// Chrome.
// TODO(b/342089839): Add a public API.
// TODO(b/342089839): Add the sync bridge as a member.
class PlusAddressSettingService : public KeyedService {
 public:
  PlusAddressSettingService() = default;
  ~PlusAddressSettingService() override = default;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_SETTINGS_PLUS_ADDRESS_SETTING_SERVICE_H_
