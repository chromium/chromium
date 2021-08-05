// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/net/in_process_network_connection_tracker.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestNetworkConnectionObserver
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  explicit TestNetworkConnectionObserver(
      network::NetworkConnectionTracker* tracker)
      : tracker_(tracker) {
    tracker_->AddNetworkConnectionObserver(this);
  }

  ~TestNetworkConnectionObserver() override {
    tracker_->RemoveNetworkConnectionObserver(this);
  }

  void OnConnectionChanged(network::mojom::ConnectionType type) override {
    call_count++;
    last_type = type;
  }

  int call_count = 0;
  network::mojom::ConnectionType last_type =
      network::mojom::ConnectionType::CONNECTION_UNKNOWN;

 private:
  network::NetworkConnectionTracker* tracker_;
};

class InProcessNetworkConnectionTrackerTest : public ::testing::Test {
 protected:
  void SetConnectionType(net::NetworkChangeNotifier::ConnectionType type) {
    notifier_->SetConnectionType(type);
    notifier_->NotifyObserversOfNetworkChangeForTests(
        notifier_->GetConnectionType());
    task_environment_.RunUntilIdle();
  }

  network::NetworkConnectionTracker::ConnectionTypeCallback
  UnreachedCallback() {
    return base::BindOnce(
        [](network::mojom::ConnectionType type) { NOTREACHED(); });
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::test::MockNetworkChangeNotifier> notifier_ =
      net::test::MockNetworkChangeNotifier::Create();
};

// Tests that a registered observer gets called.
TEST_F(InProcessNetworkConnectionTrackerTest, ObserverCalled) {
  InProcessNetworkConnectionTracker tracker;
  TestNetworkConnectionObserver observer(&tracker);

  ASSERT_EQ(observer.call_count, 0);
  SetConnectionType(net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G);
  ASSERT_EQ(observer.call_count, 1);
  ASSERT_EQ(observer.last_type, network::mojom::ConnectionType::CONNECTION_3G);
  SetConnectionType(net::NetworkChangeNotifier::ConnectionType::CONNECTION_4G);
  ASSERT_EQ(observer.call_count, 2);
  ASSERT_EQ(observer.last_type, network::mojom::ConnectionType::CONNECTION_4G);
}

// Tests that GetConnectionType returns synchronously.
TEST_F(InProcessNetworkConnectionTrackerTest, GetConnectionTypeSync) {
  SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
  InProcessNetworkConnectionTracker tracker;

  auto type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  bool sync = tracker.GetConnectionType(&type, UnreachedCallback());
  ASSERT_TRUE(sync);
  ASSERT_EQ(type, network::mojom::ConnectionType::CONNECTION_WIFI);

  SetConnectionType(net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G);
  sync = tracker.GetConnectionType(&type, UnreachedCallback());
  ASSERT_TRUE(sync);
  ASSERT_EQ(type, network::mojom::ConnectionType::CONNECTION_2G);
}
