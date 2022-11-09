// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/net/network.h"

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/unittest_util.h"
#include "components/update_client/network.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include <Urlmon.h>
#endif

namespace updater {
namespace {

// Sets up a download for "chrome/updater/test/data/signed.exe" using the
// embedded test server running on localhost.
class UpdaterDownloadTest : public ::testing::Test {
 protected:
  ~UpdaterDownloadTest() override = default;

  base::test::TaskEnvironment task_environment_;
  net::test_server::EmbeddedTestServer server_;
  net::test_server::EmbeddedTestServerHandle server_handle_;
  base::ScopedTempDir scoped_dir_;
  base::FilePath dest_;
  GURL gurl_;

 private:
  void SetUp() override {
    server_.ServeFilesFromSourceDirectory("chrome/updater/test/data");
    server_handle_ = server_.StartAndReturnHandle();
    ASSERT_TRUE(scoped_dir_.CreateUniqueTempDir());
    dest_ = scoped_dir_.GetPath().AppendASCII("updater-signed.exe");
    gurl_ = GURL(base::StrCat({server_.base_url().spec(), "signed.exe"}));
  }
};

}  // namespace

#if BUILDFLAG(IS_WIN)
// Tests that a direct download through URL moniker from a local HTTP server is
// reasonably fast. This provides a baseline to compare the network throughput
// of various fetchers.
TEST_F(UpdaterDownloadTest, URLMonFetcher) {
  EXPECT_FALSE(base::PathExists(dest_));
  EXPECT_HRESULT_SUCCEEDED(
      ::URLDownloadToFile(nullptr, base::ASCIIToWide(gurl_.spec()).c_str(),
                          dest_.value().c_str(), 0, nullptr));
  EXPECT_TRUE(base::PathExists(dest_));
}
#endif

TEST_F(UpdaterDownloadTest, NetworkFetcher) {
  EXPECT_FALSE(base::PathExists(dest_));
  {
    // The fetcher must not block the sequence when downloading.
    base::ScopedDisallowBlocking no_blocking_allowed_on_sequence;

    base::RunLoop run_loop;
    auto factory = base::MakeRefCounted<NetworkFetcherFactory>(
        PolicyServiceProxyConfiguration::Get(test::CreateTestPolicyService()));
    ASSERT_NE(factory, nullptr);
    std::unique_ptr<update_client::NetworkFetcher> fetcher = factory->Create();
    ASSERT_NE(fetcher, nullptr);
    fetcher->DownloadToFile(
        gurl_, dest_,
        base::BindOnce([](int response_code, int64_t /*content_length*/) {
          EXPECT_EQ(response_code, 200);
        }),
        base::BindRepeating([](int64_t /*current*/) {}),
        base::BindOnce([](int net_error, int64_t content_size) {
          EXPECT_EQ(net_error, 0);
        }).Then(run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_TRUE(base::PathExists(dest_));
}

}  // namespace updater
