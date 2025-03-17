// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/version_history_client.h"

#include <string_view>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_browser_process.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

class VersionHistoryClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        url_loader_factory_.GetSafeWeakWrapper());
  }

  network::TestURLLoaderFactory& url_loader_factory() {
    return url_loader_factory_;
  }

  void SetAPIResponse(const GURL& url,
                      net::HttpStatusCode status_code,
                      std::string_view content) {
    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(status_code);
    head->mime_type = "application/json";
    network::URLLoaderCompletionStatus status;
    url_loader_factory().AddResponse(url, std::move(head), content, status);
  }

 private:
  base::test::TaskEnvironment task_environment_;

  network::TestURLLoaderFactory url_loader_factory_;
};

#if BUILDFLAG(IS_WIN)

#if defined(ARCH_CPU_ARM64)
#define CURRENT_PLATFORM "win_arm64"
#elif defined(ARCH_CPU_X86_64)
#define CURRENT_PLATFORM "win64"
#else
#define CURRENT_PLATFORM "win"
#endif

#elif BUILDFLAG(IS_LINUX)

#define CURRENT_PLATFORM "linux"

#elif BUILDFLAG(IS_MAC)

#if defined(ARCH_CPU_ARM64)
#define CURRENT_PLATFORM "mac_arm64"
#else
#define CURRENT_PLATFORM "mac"
#endif

#elif BUILDFLAG(IS_CHROMEOS)

#define CURRENT_PLATFORM "chromeos"

#else

#error Unsupported platform

#endif  // BUILDFLAG(IS_WIN)

// Tests that GetLastServedDate() returns the correct date when the server is
// responsive.
TEST_F(VersionHistoryClientTest, GetLastServedDateSimple) {
  SetAPIResponse(GURL("https://versionhistory.googleapis.com/v1/chrome/"
                      "platforms/" CURRENT_PLATFORM
                      "/channels/stable/versions/131.0.6778.205/releases/"
                      "?order_by=endtime%20desc&page_size=1"),
                 net::HTTP_OK, R"({
        "releases": [
          {
            "name": "chrome/platforms/win/channels/stable/versions/131.0.6778.205/releases/1736364451",
            "serving": {
              "startTime": "2025-01-08T19:27:31.216008Z",
              "endTime": "2025-01-09T21:01:31.167025Z"
            },
            "fraction": 0.4975,
            "version": "131.0.6778.205",
            "fractionGroup": "144",
            "pinnable": false
          }
        ]
      })");

  base::Time expected_time;
  ASSERT_TRUE(
      base::Time::FromUTCString("2025-01-09T21:01:31.167025Z", &expected_time));

  base::RunLoop run_loop;
  LastServedDateCallback callback =
      base::BindLambdaForTesting([&](std::optional<base::Time> last_served) {
        EXPECT_EQ(expected_time, last_served);
        run_loop.Quit();
      });
  GetLastServedDate(base::Version("131.0.6778.205"), std::move(callback));
  run_loop.Run();
}

// Tests that GetLastServedDate() returns nullopt when the server returns a 404.
TEST_F(VersionHistoryClientTest, GetLastServedDate404) {
  SetAPIResponse(GURL("https://versionhistory.googleapis.com/v1/chrome/"
                      "platforms/" CURRENT_PLATFORM
                      "/channels/stable/versions/131.0.6778.205/releases/"
                      "?order_by=endtime%20desc&page_size=1"),
                 net::HTTP_NOT_FOUND, "");

  base::RunLoop run_loop;
  LastServedDateCallback callback =
      base::BindLambdaForTesting([&](std::optional<base::Time> last_served) {
        EXPECT_EQ(std::nullopt, last_served);
        run_loop.Quit();
      });
  GetLastServedDate(base::Version("131.0.6778.205"), std::move(callback));
  run_loop.Run();
}

// Tests that GetLastServedDate() returns nullopt when the server returns
// invalid JSON.
TEST_F(VersionHistoryClientTest, GetLastServedDateInvalidJSON) {
  SetAPIResponse(GURL("https://versionhistory.googleapis.com/v1/chrome/"
                      "platforms/" CURRENT_PLATFORM
                      "/channels/stable/versions/131.0.6778.205/releases/"
                      "?order_by=endtime%20desc&page_size=1"),
                 net::HTTP_OK, "invalid JSON");

  base::RunLoop run_loop;
  LastServedDateCallback callback =
      base::BindLambdaForTesting([&](std::optional<base::Time> last_served) {
        EXPECT_EQ(std::nullopt, last_served);
        run_loop.Quit();
      });
  GetLastServedDate(base::Version("131.0.6778.205"), std::move(callback));
  run_loop.Run();
}

// Tests that GetLastServedDate() returns nullopt when the server returns a
// response with a missing endTime.
TEST_F(VersionHistoryClientTest, GetLastServedDateMissingEndTime) {
  SetAPIResponse(GURL("https://versionhistory.googleapis.com/v1/chrome/"
                      "platforms/" CURRENT_PLATFORM
                      "/channels/stable/versions/131.0.6778.205/releases/"
                      "?order_by=endtime%20desc&page_size=1"),
                 net::HTTP_OK, R"({
        "releases": [
          {
            "name": "chrome/platforms/win/channels/stable/versions/131.0.6778.205/releases/1736364451",
            "serving": {
              "startTime": "2025-01-08T19:27:31.216008Z"
            },
            "fraction": 0.4975,
            "version": "131.0.6778.205",
            "fractionGroup": "144",
            "pinnable": false
          }
        ]
      })");

  base::RunLoop run_loop;
  LastServedDateCallback callback =
      base::BindLambdaForTesting([&](std::optional<base::Time> last_served) {
        EXPECT_EQ(std::nullopt, last_served);
        run_loop.Quit();
      });
  GetLastServedDate(base::Version("131.0.6778.205"), std::move(callback));
  run_loop.Run();
}

// Tests that GetLastServedDate() returns nullopt when the server returns a
// response with an empty releases array.
TEST_F(VersionHistoryClientTest, GetLastServedDateEmptyReleases) {
  SetAPIResponse(GURL("https://versionhistory.googleapis.com/v1/chrome/"
                      "platforms/" CURRENT_PLATFORM
                      "/channels/stable/versions/131.0.6778.205/releases/"
                      "?order_by=endtime%20desc&page_size=1"),
                 net::HTTP_OK, R"({"releases":[]})");

  base::RunLoop run_loop;
  LastServedDateCallback callback =
      base::BindLambdaForTesting([&](std::optional<base::Time> last_served) {
        EXPECT_EQ(std::nullopt, last_served);
        run_loop.Quit();
      });
  GetLastServedDate(base::Version("131.0.6778.205"), std::move(callback));
  run_loop.Run();
}
