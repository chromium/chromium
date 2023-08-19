// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/wait_for_network_callback_helper_ios.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

// A test fixture to test IOSWaitForNetworkCallbackHelper.
class WaitForNetworkCallbackHelperTestIOS : public testing::Test {
 public:
  void CallbackFunction() { num_callbacks_invoked_++; }

 protected:
  WaitForNetworkCallbackHelperTestIOS() : num_callbacks_invoked_(0) {}

  int num_callbacks_invoked_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      network_change_notifier_ = net::test::MockNetworkChangeNotifier::Create();
  WaitForNetworkCallbackHelperIOS callback_helper_;
};

TEST_F(WaitForNetworkCallbackHelperTestIOS, CallbackInvokedImmediately) {
  network_change_notifier_->SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
  callback_helper_.DelayNetworkCall(
      base::BindOnce(&WaitForNetworkCallbackHelperTestIOS::CallbackFunction,
                     base::Unretained(this)));
  EXPECT_EQ(1, num_callbacks_invoked_);
}

TEST_F(WaitForNetworkCallbackHelperTestIOS, CallbackInvokedLater) {
  network_change_notifier_->SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE);
  callback_helper_.DelayNetworkCall(
      base::BindOnce(&WaitForNetworkCallbackHelperTestIOS::CallbackFunction,
                     base::Unretained(this)));
  callback_helper_.DelayNetworkCall(
      base::BindOnce(&WaitForNetworkCallbackHelperTestIOS::CallbackFunction,
                     base::Unretained(this)));
  EXPECT_EQ(0, num_callbacks_invoked_);

  network_change_notifier_->SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
  network_change_notifier_->NotifyObserversOfConnectionTypeChangeForTests(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2, num_callbacks_invoked_);
}
