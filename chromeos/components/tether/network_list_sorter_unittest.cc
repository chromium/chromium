// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/network_list_sorter.h"

#include <memory>

#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/tether_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace tether {

namespace {

const char kGuid0[] = "guid0";
const char kGuid1[] = "guid1";
const char kGuid2[] = "guid2";

const char* const kGuidArray[] = {kGuid0, kGuid1, kGuid2};

}  // namespace

class NetworkListSorterTest : public testing::Test {
 protected:
  NetworkListSorterTest() = default;

  void SetUp() override {
    network_list_sorter_ = std::make_unique<NetworkListSorter>();
  }

  void GenerateTestList() {
    list_ = std::make_unique<NetworkStateHandler::ManagedStateList>();

    auto state0 = std::make_unique<NetworkState>(kGuid0);
    state0->SetGuid(kGuid0);
    state0->set_visible(true);
    list_->emplace_back(std::move(state0));

    auto state1 = std::make_unique<NetworkState>(kGuid1);
    state1->SetGuid(kGuid1);
    state1->set_visible(true);
    list_->emplace_back(std::move(state1));

    auto state2 = std::make_unique<NetworkState>(kGuid2);
    state2->SetGuid(kGuid2);
    state2->set_visible(true);
    list_->emplace_back(std::move(state2));
  }

  NetworkState* NetworkAtIndex(int index) {
    return list_->at(index)->AsNetworkState();
  }

  void SetName(NetworkState* state, const std::string& name) {
    state->set_name(name);
  }

  void SortAndVerifySortOrder(int el0, int el1, int el2) {
    network_list_sorter_->SortTetherNetworkList(list_.get());

    EXPECT_EQ(NetworkAtIndex(0)->guid(), kGuidArray[el0]);
    EXPECT_EQ(NetworkAtIndex(1)->guid(), kGuidArray[el1]);
    EXPECT_EQ(NetworkAtIndex(2)->guid(), kGuidArray[el2]);
  }

  std::unique_ptr<NetworkStateHandler::ManagedStateList> list_;

  std::unique_ptr<NetworkListSorter> network_list_sorter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkListSorterTest);
};

TEST_F(NetworkListSorterTest, ConnectionState) {
  GenerateTestList();
  NetworkAtIndex(0)->set_connection_state_for_testing(shill::kStateIdle);
  NetworkAtIndex(1)->set_connection_state_for_testing(shill::kStateAssociation);
  NetworkAtIndex(2)->set_connection_state_for_testing(shill::kStateOnline);
  SortAndVerifySortOrder(2, 1, 0);
}

TEST_F(NetworkListSorterTest, SignalStrength) {
  GenerateTestList();
  NetworkAtIndex(0)->set_signal_strength(0);
  NetworkAtIndex(1)->set_signal_strength(50);
  NetworkAtIndex(2)->set_signal_strength(100);
  SortAndVerifySortOrder(2, 1, 0);
}

TEST_F(NetworkListSorterTest, BatteryPercentage) {
  GenerateTestList();
  NetworkAtIndex(0)->set_signal_strength(0);
  NetworkAtIndex(1)->set_signal_strength(50);
  NetworkAtIndex(2)->set_signal_strength(100);
  SortAndVerifySortOrder(2, 1, 0);
}

TEST_F(NetworkListSorterTest, HasConnectedToHost) {
  GenerateTestList();
  NetworkAtIndex(2)->set_tether_has_connected_to_host(true);
  SortAndVerifySortOrder(2, 0, 1);
}

TEST_F(NetworkListSorterTest, Name) {
  GenerateTestList();
  SetName(NetworkAtIndex(0), "c");
  SetName(NetworkAtIndex(1), "b");
  SetName(NetworkAtIndex(2), "a");
  SortAndVerifySortOrder(2, 1, 0);
}

TEST_F(NetworkListSorterTest, Carrier) {
  GenerateTestList();
  NetworkAtIndex(0)->set_tether_carrier("c");
  NetworkAtIndex(1)->set_tether_carrier("b");
  NetworkAtIndex(2)->set_tether_carrier("a");
  SortAndVerifySortOrder(2, 1, 0);
}

TEST_F(NetworkListSorterTest, Guid) {
  GenerateTestList();
  SortAndVerifySortOrder(0, 1, 2);
}

}  // namespace tether

}  // namespace chromeos
