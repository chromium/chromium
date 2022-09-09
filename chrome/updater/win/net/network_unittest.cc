// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/net/network.h"

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/unittest_util.h"
#include "chrome/updater/win/net/network_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(UpdaterTestNetwork, NetworkFetcherWinHTTPFactory) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);
  std::unique_ptr<update_client::NetworkFetcher> fetcher =
      base::MakeRefCounted<NetworkFetcherFactory>(
          PolicyServiceProxyConfiguration::Get(test::CreateTestPolicyService()))
          ->Create();
  EXPECT_NE(fetcher, nullptr);
}

}  // namespace updater
