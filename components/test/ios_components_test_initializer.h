// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_IOS_COMPONENTS_TEST_INITIALIZER_H_
#define COMPONENTS_TEST_IOS_COMPONENTS_TEST_INITIALIZER_H_

#include <memory>

namespace network {
class TestNetworkConnectionTracker;
}

// Contains common initialization logic needed by ios component tests.
class IosComponentsTestInitializer {
 public:
  IosComponentsTestInitializer();
  IosComponentsTestInitializer(const IosComponentsTestInitializer&) = delete;
  IosComponentsTestInitializer& operator=(const IosComponentsTestInitializer&) =
      delete;
  virtual ~IosComponentsTestInitializer();

 private:
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
};

#endif  // COMPONENTS_TEST_IOS_COMPONENTS_TEST_INITIALIZER_H_
