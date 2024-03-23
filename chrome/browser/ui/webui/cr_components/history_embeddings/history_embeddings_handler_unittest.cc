// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/history_embeddings/history_embeddings_handler.h"

#include "base/test/mock_callback.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/cr_components/history_embeddings/history_embeddings.mojom.h"

class HistoryEmbeddingsHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ =
        profile_manager_->CreateTestingProfile("History Embeddings Test User");

    handler_ = std::make_unique<HistoryEmbeddingsHandler>(
        mojo::PendingReceiver<history_embeddings::mojom::PageHandler>(),
        profile_->GetWeakPtr());
  }

  void TearDown() override { handler_.reset(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<HistoryEmbeddingsHandler> handler_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
};

TEST_F(HistoryEmbeddingsHandlerTest, DoesSomething) {
  base::MockCallback<HistoryEmbeddingsHandler::DoSomethingCallback> callback;
  bool did_something = false;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](bool response) { did_something = response; }));
  handler_->DoSomething(callback.Get());
  ASSERT_TRUE(did_something);
}

TEST_F(HistoryEmbeddingsHandlerTest, Searches) {
  base::MockCallback<HistoryEmbeddingsHandler::SearchCallback> callback;
  size_t result_size = -1;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](history_embeddings::mojom::SearchResultPtr result) {
            result_size = result->items.size();
          }));
  auto query = history_embeddings::mojom::SearchQuery::New();
  query->query = "search query for empty result";
  ASSERT_NE(result_size, 0u);
  handler_->Search(std::move(query), callback.Get());
  ASSERT_EQ(result_size, 0u);
}
