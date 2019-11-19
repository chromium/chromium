// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/fake_connectivity_checker.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromecast {

FakeConnectivityChecker::FakeConnectivityChecker()
    : ConnectivityChecker(base::ThreadTaskRunnerHandle::Get()),
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
