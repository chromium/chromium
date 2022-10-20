// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/net/network.h"

#include <Urlmon.h>

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/unittest_util.h"
#include "chrome/updater/win/net/network_fetcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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

// Tests that a direct download through URL moniker from a local HTTP server is
// reasonably fast. This provides a baseline to compare the network throughput
// of various fetchers.
TEST(UpdaterTestNetwork, URLMonFetcher) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);
  net::test_server::EmbeddedTestServer server;
  server.ServeFilesFromSourceDirectory("chrome/updater/test/data");
  net::test_server::EmbeddedTestServerHandle server_handle =
      server.StartAndReturnHandle();
  base::ScopedTempDir scoped_dir;
  ASSERT_TRUE(scoped_dir.CreateUniqueTempDir());
  const base::FilePath dest =
      scoped_dir.GetPath().AppendASCII("updater-signed.exe");
  EXPECT_HRESULT_SUCCEEDED(::URLDownloadToFile(
      nullptr,
      base::ASCIIToWide(base::StrCat({server.base_url().spec(), "signed.exe"}))
          .c_str(),
      dest.value().c_str(), 0, nullptr));
  EXPECT_TRUE(base::PathExists(dest));
}

}  // namespace updater
