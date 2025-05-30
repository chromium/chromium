// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/pix_account_linking_manager.h"

#include <memory>

#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/pix_account_linking_manager_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {

class PixAccountLinkingManagerTest : public testing::Test {
 public:
  PixAccountLinkingManagerTest()
      : manager_(std::make_unique<PixAccountLinkingManager>(&client_)) {}

 protected:
  MockFacilitatedPaymentsClient& client() { return client_; }
  PixAccountLinkingManager* manager() { return manager_.get(); }
  inline PixAccountLinkingManagerTestApi test_api() {
    return PixAccountLinkingManagerTestApi(manager_.get());
  }

 private:
  // Order matters here because `manager_` keeps a reference to `client_`.
  MockFacilitatedPaymentsClient client_;
  std::unique_ptr<PixAccountLinkingManager> manager_;
};

TEST_F(PixAccountLinkingManagerTest, SuccessPathShowsPrompt) {
  // Success path setup.
  ON_CALL(client(), IsPixAccountLinkingSupported)
      .WillByDefault(testing::Return(true));

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt);

  manager()->MaybeShowPixAccountLinkingPrompt();
}

TEST_F(PixAccountLinkingManagerTest,
       PixAccountLinkingNotSupported_PromptNotShown) {
  ON_CALL(client(), IsPixAccountLinkingSupported)
      .WillByDefault(testing::Return(false));

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt();
}

TEST_F(PixAccountLinkingManagerTest, OnAccepted) {
  EXPECT_CALL(client(), OnPixAccountLinkingPromptAccepted);

  test_api().OnAccepted();
}

}  // namespace payments::facilitated
