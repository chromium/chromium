// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/pix_account_linking_manager.h"

#include <memory>

#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/pix_account_linking_manager_test_api.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {

class PixAccountLinkingManagerTest : public testing::Test {
 public:
  PixAccountLinkingManagerTest()
      : manager_(std::make_unique<PixAccountLinkingManager>(&client_)) {}

  void SetUp() override {
    pref_service_ = autofill::test::PrefServiceForTesting();
    payments_data_manager_ =
        std::make_unique<autofill::TestPaymentsDataManager>();
    payments_data_manager_->SetPrefService(pref_service_.get());
    payments_data_manager_->SetSyncServiceForTest(&sync_service_);
    ON_CALL(client_, GetPaymentsDataManager)
        .WillByDefault(testing::Return(payments_data_manager_.get()));

    // Success path setup. The Pix account linking user pref is default enabled.
    ON_CALL(client(), IsPixAccountLinkingSupported)
        .WillByDefault(testing::Return(true));
  }

  void TearDown() override {
    payments_data_manager_->ClearAllServerDataForTesting();
    payments_data_manager_.reset();
  }

 protected:
  MockFacilitatedPaymentsClient& client() { return client_; }
  PixAccountLinkingManager* manager() { return manager_.get(); }
  inline PixAccountLinkingManagerTestApi test_api() {
    return PixAccountLinkingManagerTestApi(manager_.get());
  }

  std::unique_ptr<PrefService> pref_service_;

 private:
  // Order matters here because `manager_` keeps a reference to `client_`.
  MockFacilitatedPaymentsClient client_;
  std::unique_ptr<PixAccountLinkingManager> manager_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<autofill::TestPaymentsDataManager> payments_data_manager_;
};

TEST_F(PixAccountLinkingManagerTest, SuccessPathShowsPrompt) {
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

TEST_F(PixAccountLinkingManagerTest,
       PixAccountLinkingPrefDisabled_PromptNotShown) {
  autofill::prefs::SetFacilitatedPaymentsPixAccountLinking(pref_service_.get(),
                                                           false);

  EXPECT_CALL(client(), ShowPixAccountLinkingPrompt).Times(0);

  manager()->MaybeShowPixAccountLinkingPrompt();
}

TEST_F(PixAccountLinkingManagerTest, OnAccepted) {
  EXPECT_CALL(client(), OnPixAccountLinkingPromptAccepted);

  test_api().OnAccepted();
}

}  // namespace payments::facilitated
