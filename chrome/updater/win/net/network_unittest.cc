// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/net/network.h"

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/win/net/network_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(UpdaterTestNetwork, NetworkFetcherWinHTTPFactory) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);

  PolicyService::PolicyManagerVector managers;
  managers.push_back(GetPolicyManager());
  auto policy_service =
      base::MakeRefCounted<PolicyService>(std::move(managers));

  auto fetcher =
      base::MakeRefCounted<NetworkFetcherFactory>(policy_service)->Create();
  EXPECT_NE(nullptr, fetcher.get());
}

}  // namespace updater
