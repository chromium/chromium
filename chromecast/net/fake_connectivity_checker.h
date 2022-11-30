// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_FAKE_CONNECTIVITY_CHECKER_H_
#define CHROMECAST_NET_FAKE_CONNECTIVITY_CHECKER_H_

#include "chromecast/net/connectivity_checker.h"

namespace chromecast {

// A simple fake connectivity checker for testing. Will appeared to be
// connected by default.
class FakeConnectivityChecker : public ConnectivityChecker {
 public:
  FakeConnectivityChecker();

  FakeConnectivityChecker(const FakeConnectivityChecker&) = delete;
  FakeConnectivityChecker& operator=(const FakeConnectivityChecker&) = delete;

  // ConnectivityChecker implementation:
  bool Connected() const override;
  void Check() override;

  // Sets connectivity and notifies observers if it has changed.
  void SetConnectedForTest(bool connected);

 protected:
  ~FakeConnectivityChecker() override;

 private:
  friend class base::RefCountedThreadSafe<FakeConnectivityChecker>;
  bool connected_;
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_FAKE_CONNECTIVITY_CHECKER_H_
