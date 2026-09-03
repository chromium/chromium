// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/request_header_integrity/chrome_companero_loader.h"

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "chrome/common/request_header_integrity/chrome_companero.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/http_request_headers.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace request_header_integrity {

class ChromeCompaneroLoaderTest : public testing::Test {
 protected:
  static void ResetLoaderForTesting() {
    auto& instance = ChromeCompaneroLoader::GetInstance();
    base::AutoLock lock(instance.cache_lock_);
    instance.cached_header_name_.clear();
    instance.cached_value_.clear();
    instance.cached_value_time_ = base::TimeTicks();
    instance.companero_remote_.reset();
    instance.refresh_timer_.Stop();
  }

  static void SetCacheForTesting(const std::string& name,
                                 const std::string& value) {
    auto& instance = ChromeCompaneroLoader::GetInstance();
    base::AutoLock lock(instance.cache_lock_);
    instance.cached_header_name_ = name;
    instance.cached_value_ = value;
    instance.cached_value_time_ = base::TimeTicks::Now();
  }
};

class MockChromeCompanero
    : public request_header_integrity::mojom::ChromeCompanero {
 public:
  MockChromeCompanero() = default;
  ~MockChromeCompanero() override = default;

  void GetHeaderNameAndValue(GetHeaderNameAndValueCallback callback) override {
    call_count_++;
    if (header_name_.empty() && value_.empty()) {
      std::move(callback).Run(nullptr);
    } else {
      std::move(callback).Run(
          network::mojom::HttpRequestHeaderKeyValuePair::New(header_name_,
                                                             value_));
    }
  }

  void set_response(const std::string& header_name, const std::string& value) {
    header_name_ = header_name;
    value_ = value;
  }

  int call_count() const { return call_count_; }

 private:
  std::string header_name_;
  std::string value_;
  int call_count_ = 0;
};

class RequestHeaderIntegrityRendererTest : public ChromeCompaneroLoaderTest {
 protected:
  void SetUp() override {
    ResetLoaderForTesting();
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII("type",
                                                                    "renderer");
  }

  void TearDown() override { ResetLoaderForTesting(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedCommandLine scoped_command_line_;
  MockChromeCompanero mock_companero_;
  mojo::Receiver<request_header_integrity::mojom::ChromeCompanero> receiver_{
      &mock_companero_};
};

TEST_F(RequestHeaderIntegrityRendererTest, MojoSuccess) {
  mock_companero_.set_response("X-Integrity-Header",
                               "mocked_token_value_12345678");

  ChromeCompaneroLoader::GetInstance().SetMojoRemote(
      receiver_.BindNewPipeAndPassRemote());

  // Wait for the first RefreshToken to complete.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(1, mock_companero_.call_count());

  auto result = ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("X-Integrity-Header", result->name);
  EXPECT_EQ("mocked_token_value_12345678", result->value);
}

TEST_F(RequestHeaderIntegrityRendererTest, MojoFailureEmptyResponse) {
  mock_companero_.set_response("", "");

  ChromeCompaneroLoader::GetInstance().SetMojoRemote(
      receiver_.BindNewPipeAndPassRemote());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(1, mock_companero_.call_count());

  // Should return nullopt because response was empty.
  EXPECT_EQ(ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue(),
            std::nullopt);
}

TEST_F(RequestHeaderIntegrityRendererTest, MojoFailureNoBinding) {
  // Should return nullopt because remote is not bound.
  EXPECT_EQ(ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue(),
            std::nullopt);
}

TEST_F(RequestHeaderIntegrityRendererTest, MojoTimerRefresh) {
  mock_companero_.set_response("X-Integrity-Header", "token_1");

  ChromeCompaneroLoader::GetInstance().SetMojoRemote(
      receiver_.BindNewPipeAndPassRemote());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(1, mock_companero_.call_count());

  auto result1 = ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue();
  ASSERT_TRUE(result1.has_value());
  EXPECT_EQ("token_1", result1->value);

  // Set new response for the next refresh.
  mock_companero_.set_response("X-Integrity-Header", "token_2");

  // Fast-forward by 1 minute (timer delay is now 1 minute).
  task_environment_.FastForwardBy(base::Minutes(1));

  // The timer should have fired and requested a new token.
  EXPECT_EQ(2, mock_companero_.call_count());

  auto result2 = ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue();
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ("token_2", result2->value);
}

TEST_F(RequestHeaderIntegrityRendererTest,
       MojoCacheExpirationOnConnectionLoss) {
  mock_companero_.set_response("X-Integrity-Header", "token_1");

  ChromeCompaneroLoader::GetInstance().SetMojoRemote(
      receiver_.BindNewPipeAndPassRemote());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(1, mock_companero_.call_count());

  auto result1 = ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue();
  ASSERT_TRUE(result1.has_value());
  EXPECT_EQ("token_1", result1->value);

  // Disconnect the Mojo binding to simulate connection loss.
  receiver_.reset();
  task_environment_.RunUntilIdle();

  // Fast-forward by 1.5 minutes. Cache TTL is 2 minutes, so it should still be
  // valid.
  task_environment_.FastForwardBy(base::Seconds(90));

  auto result2 = ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue();
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ("token_1", result2->value);

  // Fast-forward by 1 more minute (total 2.5 minutes). Even though
  // cached_value_time_ has exceeded kCacheTtl, helper processes fall back to
  // returning the stale cached token.
  task_environment_.FastForwardBy(base::Seconds(60));

  auto result3 = ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue();
  ASSERT_TRUE(result3.has_value());
  EXPECT_EQ("token_1", result3->value);
}

TEST_F(RequestHeaderIntegrityRendererTest, ConcurrentCacheReads) {
  SetCacheForTesting("X-Integrity-Header", "initial_token");

  // Concurrently query GetHeaderNameAndValue() across multiple threads.
  std::vector<std::thread> reader_threads;
  for (int i = 0; i < 8; ++i) {
    reader_threads.emplace_back([]() {
      for (int j = 0; j < 100; ++j) {
        auto result =
            ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ("X-Integrity-Header", result->name);
        EXPECT_EQ("initial_token", result->value);
      }
    });
  }

  for (auto& t : reader_threads) {
    t.join();
  }
}

}  // namespace request_header_integrity
