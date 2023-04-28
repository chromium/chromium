// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/search_and_assistant_enabled_checker.h"

#include <memory>
#include <set>
#include <string>

#include "base/functional/bind.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kFalseResponse[] = R"()]}'
  {
    "result": false
  }
)";

constexpr char kTrueResponse[] = R"()]}'
  {
    "result": true
  }
)";

}  // namespace

class MockSearchAndAssistantEnabledCheckerDelegate
    : public SearchAndAssistantEnabledChecker::Delegate {
 public:
  MockSearchAndAssistantEnabledCheckerDelegate() = default;
  ~MockSearchAndAssistantEnabledCheckerDelegate() override = default;

  MOCK_METHOD(void, OnError, (), (override));
  MOCK_METHOD(void, OnSearchAndAssistantStateReceived, (bool), (override));

 private:
};

class SearchAndAssistantEnabledCheckerTest : public ChromeAshTestBase {
 public:
  SearchAndAssistantEnabledCheckerTest() = default;
  ~SearchAndAssistantEnabledCheckerTest() override = default;

  void SetUp() override {
    ChromeAshTestBase::SetUp();

    sender_ = std::make_unique<SearchAndAssistantEnabledChecker>(
        &test_url_loader_factory_, &delegate_);
  }

  void TearDown() override {
    sender_.reset();
    ChromeAshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<SearchAndAssistantEnabledChecker> sender_;
  testing::StrictMock<MockSearchAndAssistantEnabledCheckerDelegate> delegate_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(SearchAndAssistantEnabledCheckerTest, TrueResponse) {
  test_url_loader_factory_.AddResponse(
      chromeos::assistant::kSampleServiceIdRequest, kTrueResponse);
  sender_->SyncSearchAndAssistantState();
  EXPECT_CALL(delegate_, OnSearchAndAssistantStateReceived(true));
  base::RunLoop().RunUntilIdle();
}

TEST_F(SearchAndAssistantEnabledCheckerTest, FalseResponse) {
  test_url_loader_factory_.AddResponse(
      chromeos::assistant::kSampleServiceIdRequest, kFalseResponse);
  sender_->SyncSearchAndAssistantState();
  EXPECT_CALL(delegate_, OnSearchAndAssistantStateReceived(false));
  EXPECT_CALL(delegate_, OnError()).Times(0);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SearchAndAssistantEnabledCheckerTest, NetworkError) {
  test_url_loader_factory_.AddResponse(
      GURL(chromeos::assistant::kSampleServiceIdRequest),
      network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  sender_->SyncSearchAndAssistantState();
  EXPECT_CALL(delegate_, OnError());
  base::RunLoop().RunUntilIdle();
}

TEST_F(SearchAndAssistantEnabledCheckerTest, InvalidResponse) {
  test_url_loader_factory_.AddResponse(
      chromeos::assistant::kSampleServiceIdRequest, "");
  sender_->SyncSearchAndAssistantState();
  EXPECT_CALL(delegate_, OnError());
  base::RunLoop().RunUntilIdle();
}
