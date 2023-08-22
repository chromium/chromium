// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_NETWORK_METADATA_STORE_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_NETWORK_METADATA_STORE_H_

#include <string>

#include "base/containers/flat_map.h"
#include "chromeos/ash/components/network/network_metadata_store.h"

namespace ash {

class FakeNetworkMetadataStore : public NetworkMetadataStore {
 public:
  FakeNetworkMetadataStore();
  ~FakeNetworkMetadataStore() override;

  UserTextMessageSuppressionState GetUserTextMessageSuppressionState(
      const std::string& guid) override;

  void SetUserTextMessageSuppressionState(
      const std::string& guid,
      const UserTextMessageSuppressionState& state) override;

 private:
  base::flat_map<std::string, UserTextMessageSuppressionState>
      text_message_suppression_state_map_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_NETWORK_METADATA_STORE_H_
