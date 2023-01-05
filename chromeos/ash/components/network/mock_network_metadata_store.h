// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_MOCK_NETWORK_METADATA_STORE_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_MOCK_NETWORK_METADATA_STORE_H_

#include "chromeos/ash/components/network/network_metadata_store.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

// TODO(b/260743185): Update this class to mock the rest of
// NetworkMetadataStore public interface.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) MockNetworkMetadataStore
    : public NetworkMetadataStore {
 public:
  MockNetworkMetadataStore();
  MockNetworkMetadataStore(const MockNetworkMetadataStore&) = delete;
  MockNetworkMetadataStore& operator=(const MockNetworkMetadataStore&) = delete;
  ~MockNetworkMetadataStore() override;

  // MockNetworkMetadataStore:
  MOCK_METHOD1(GetCustomApnList, const base::Value::List*(const std::string&));
  MOCK_METHOD1(GetPreRevampCustomApnList,
               const base::Value::List*(const std::string&));
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_MOCK_NETWORK_METADATA_STORE_H_
