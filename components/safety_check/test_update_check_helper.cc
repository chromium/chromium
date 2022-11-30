// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safety_check/test_update_check_helper.h"

namespace safety_check {

TestUpdateCheckHelper::TestUpdateCheckHelper() = default;

TestUpdateCheckHelper::~TestUpdateCheckHelper() = default;

void TestUpdateCheckHelper::SetConnectivity(bool online) {
  online_ = online;
}

// UpdateCheckHelper implementation.
void TestUpdateCheckHelper::CheckConnectivity(
    ConnectivityCheckCallback connection_check_callback) {
  std::move(connection_check_callback).Run(online_);
}

}  // namespace safety_check
