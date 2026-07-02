// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/request_header_integrity/chrome_companero_loader.h"

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "build/branding_buildflags.h"
#include "chrome/common/request_header_integrity/buildflags.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_REQUEST_HEADER_INTEGRITY)
#include "chrome/common/request_header_integrity/chrome_companero.mojom.h"

namespace request_header_integrity {

class ChromeCompaneroLoaderTest : public testing::Test {
 protected:
  class TestLoader : public ChromeCompaneroLoader {
   public:
    TestLoader() = default;
    ~TestLoader() = default;
  };

  std::optional<std::string> TestGetHeaderName(ChromeCompaneroLoader& loader) {
    return loader.GetHeaderNameFromLib();
  }

  std::optional<std::string> TestGetHeaderValue(ChromeCompaneroLoader& loader) {
    return loader.GetHeaderValueFromLib("test_seed", "test_key", "test_ua");
  }

  static void ResetLoaderForTesting() {
    auto& instance = ChromeCompaneroLoader::GetInstance();
    base::AutoLock lock(instance.cache_lock_);
    instance.cached_header_name_.clear();
    instance.cached_value_.clear();
    instance.cached_value_time_ = base::TimeTicks();
    instance.companero_remote_.reset();
    instance.refresh_timer_.Stop();
    instance.init_called_.store(false, std::memory_order_release);
    instance.init_succeeded_.store(false, std::memory_order_release);
  }
};

TEST_F(ChromeCompaneroLoaderTest, ChromeCompaneroDynamicLoading) {
  // Verify that ChromeCompaneroLoader successfully loads the dynamic shared
  // library libchromecompaneros.so at runtime and resolves the companero
  // values.
  ChromeCompaneroLoader::GetInstance().BrowserProcessInitialize();
  auto header_name = TestGetHeaderName(ChromeCompaneroLoader::GetInstance());
  ASSERT_TRUE(header_name.has_value());
  EXPECT_THAT(*header_name, testing::Not(testing::IsEmpty()));

  ASSERT_OK_AND_ASSIGN(
      auto result,
      ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue());
  EXPECT_EQ(result.name, *header_name);
  EXPECT_EQ(result.value.length(), 32u);
}

TEST_F(ChromeCompaneroLoaderTest, LibraryAbsentGracefulFailure) {
  // Override paths to a temp directory to simulate the library being missing.
  base::ScopedPathOverride exe_override(base::DIR_EXE);
  base::ScopedPathOverride module_override(base::DIR_MODULE);

  // Bypasses GetInstance() to instantiate a private, total isolation stack
  // instance:
  TestLoader stack_loader;
  stack_loader.BrowserProcessInitialize();

  EXPECT_EQ(TestGetHeaderName(stack_loader), std::nullopt);
  EXPECT_EQ(TestGetHeaderValue(stack_loader), std::nullopt);
  EXPECT_EQ(stack_loader.GetHeaderNameAndValue(), std::nullopt);
}

TEST_F(ChromeCompaneroLoaderTest, UninitializedLookupsBailOutSafely) {
  // 1. Instantiate a fresh, uninitialized stack instance (init_succeeded_ ==
  // false)
  TestLoader stack_loader;

  // 2. Verify that network lookups return nullopt without attempting to load
  // DLL
  EXPECT_EQ(TestGetHeaderName(stack_loader), std::nullopt);
  EXPECT_EQ(TestGetHeaderValue(stack_loader), std::nullopt);
  EXPECT_EQ(stack_loader.GetHeaderNameAndValue(), std::nullopt);
}

TEST_F(ChromeCompaneroLoaderTest, SingleShotContractEnforcement) {
  TestLoader stack_loader;
  stack_loader.BrowserProcessInitialize();
  EXPECT_DEATH_IF_SUPPORTED(stack_loader.BrowserProcessInitialize(), "");
}

TEST_F(ChromeCompaneroLoaderTest, ConcurrentInitializationAndExtraction) {
  TestLoader stack_loader;

  // 1. Launch 1 background startup thread executing BrowserProcessInitialize()
  std::thread init_thread(
      [&stack_loader]() { stack_loader.BrowserProcessInitialize(); });

  // 2. Simultaneously launch 10 network worker threads calling
  // GetHeaderNameAndValue()
  std::vector<std::thread> worker_threads;
  for (int i = 0; i < 10; ++i) {
    worker_threads.emplace_back([&stack_loader]() {
      auto result = stack_loader.GetHeaderNameAndValue();
      if (result.has_value()) {
        EXPECT_EQ(32u, result->value.length());
      }
    });
  }

  init_thread.join();
  for (auto& t : worker_threads) {
    t.join();
  }

  // 3. Confirm that once Initialize() finishes, subsequent extractions succeed
  auto final_result = stack_loader.GetHeaderNameAndValue();
  ASSERT_TRUE(final_result.has_value());
  EXPECT_EQ(32u, final_result->value.length());
}

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
          request_header_integrity::mojom::HeaderNameAndValue::New(header_name_,
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

  ASSERT_OK_AND_ASSIGN(
      auto result,
      ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue());
  EXPECT_EQ("X-Integrity-Header", result.name);
  EXPECT_EQ("mocked_token_value_12345678", result.value);
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

  ASSERT_OK_AND_ASSIGN(
      auto result1,
      ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue());
  EXPECT_EQ("token_1", result1.value);

  // Set new response for the next refresh.
  mock_companero_.set_response("X-Integrity-Header", "token_2");

  // Fast-forward by 1 minute (timer delay is now 1 minute).
  task_environment_.FastForwardBy(base::Minutes(1));

  // The timer should have fired and requested a new token.
  EXPECT_EQ(2, mock_companero_.call_count());

  ASSERT_OK_AND_ASSIGN(
      auto result2,
      ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue());
  EXPECT_EQ("token_2", result2.value);
}

TEST_F(RequestHeaderIntegrityRendererTest,
       MojoCacheExpirationOnConnectionLoss) {
  mock_companero_.set_response("X-Integrity-Header", "token_1");

  ChromeCompaneroLoader::GetInstance().SetMojoRemote(
      receiver_.BindNewPipeAndPassRemote());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(1, mock_companero_.call_count());

  ASSERT_OK_AND_ASSIGN(
      auto result1,
      ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue());
  EXPECT_EQ("token_1", result1.value);

  // Disconnect the Mojo binding to simulate connection loss.
  receiver_.reset();
  task_environment_.RunUntilIdle();

  // Fast-forward by 1.5 minutes. Cache TTL is 2 minutes, so it should still be
  // valid.
  task_environment_.FastForwardBy(base::Seconds(90));

  ASSERT_OK_AND_ASSIGN(
      auto result2,
      ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue());
  EXPECT_EQ("token_1", result2.value);

  // Fast-forward by 1 more minute (total 2.5 minutes). Even though
  // cached_value_time_ has exceeded kCacheTtl, helper processes fall back to
  // returning the stale cached token.
  task_environment_.FastForwardBy(base::Seconds(60));

  ASSERT_OK_AND_ASSIGN(
      auto result3,
      ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue());
  EXPECT_EQ("token_1", result3.value);
}

}  // namespace request_header_integrity

#else  // BUILDFLAG(ENABLE_REQUEST_HEADER_INTEGRITY)

namespace request_header_integrity {

TEST(ChromeCompaneroLoaderStubTest, PublicInterfaceStub) {
  ChromeCompaneroLoader::GetInstance().BrowserProcessInitialize();
  EXPECT_EQ(ChromeCompaneroLoader::GetInstance().GetHeaderNameAndValue(),
            std::nullopt);
}

}  // namespace request_header_integrity

#endif  // BUILDFLAG(ENABLE_REQUEST_HEADER_INTEGRITY)
