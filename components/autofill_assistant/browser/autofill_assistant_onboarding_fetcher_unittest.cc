// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/autofill_assistant_onboarding_fetcher.h"

#include "base/containers/flat_map.h"
#include "base/test/task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_assistant {
namespace {

constexpr char kTestResponseContent[] =
    R"json(
{
  "BUY_MOVIE_TICKETS": {
    "onboarding_title": "Title",
    "onboarding_text": "Text"
  }
}
    )json";

constexpr char kExpectedUrl[] =
    "https://www.gstatic.com/autofill_assistant/en-US/"
    "onboarding_definition.json";

class AutofillAssistantOnboardingFetcherTest : public ::testing::Test {
 public:
  void SetUp() override {
    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_.get());
    fetcher_ = std::make_unique<AutofillAssistantOnboardingFetcher>(
        test_shared_loader_factory_);
  }

  void TearDown() override { EXPECT_EQ(0, GetNumberOfPendingRequests()); }

  int GetNumberOfPendingRequests() {
    return test_url_loader_factory_->NumPending();
  }

  AutofillAssistantOnboardingFetcher* fetcher() { return fetcher_.get(); }

  void SimulateResponse() { SimulateResponseWithContent(kTestResponseContent); }

  void SimulateResponseWithContent(const std::string& content) {
    EXPECT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
        kExpectedUrl, content));
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<AutofillAssistantOnboardingFetcher> fetcher_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

TEST_F(AutofillAssistantOnboardingFetcherTest, FetchOnboardingStrings) {
  AutofillAssistantOnboardingFetcher::ResponseCallback callback =
      base::BindOnce([](const base::flat_map<std::string, std::string>& map) {
        EXPECT_EQ(map.at("onboarding_title"), "Title");
        EXPECT_EQ(map.at("onboarding_text"), "Text");
      });

  fetcher()->FetchOnboardingDefinition("BUY_MOVIE_TICKETS", "en-US", 300,
                                       std::move(callback));
  EXPECT_EQ(1, GetNumberOfPendingRequests());
  SimulateResponse();
  EXPECT_EQ(0, GetNumberOfPendingRequests());
}

}  // namespace
}  // namespace autofill_assistant
