// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/fake_connectivity_checker.h"
#include "base/task/single_thread_task_runner.h"

namespace chromecast {

FakeConnectivityChecker::FakeConnectivityChecker()
    : ConnectivityChecker(base::SingleThreadTaskRunner::GetCurrentDefault()),
      connected_(true) {}

FakeConnectivityChecker::~FakeConnectivityChecker() {}

bool FakeConnectivityChecker::Connected() const {
  return connected_;
}

void FakeConnectivityChecker::Check() {
}

void FakeConnectivityChecker::SetConnectedForTest(bool connected) {
  if (connected_ == connected)
    return;

  connected_ = connected;
  Notify(connected);
}

}  // namespace chromecast
