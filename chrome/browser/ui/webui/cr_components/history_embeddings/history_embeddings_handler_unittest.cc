// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/history_embeddings/history_embeddings_handler.h"

#include "base/test/mock_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/cr_components/history_embeddings/history_embeddings.mojom.h"

class HistoryEmbeddingsHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    handler_ = std::make_unique<HistoryEmbeddingsHandler>(
        mojo::PendingReceiver<history_embeddings::mojom::PageHandler>());
  }

  void TearDown() override { handler_.reset(); }

 protected:
  std::unique_ptr<HistoryEmbeddingsHandler> handler_;
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
