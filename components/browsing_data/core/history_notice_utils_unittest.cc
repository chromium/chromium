// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/history_notice_utils.h"

#include <memory>

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/history/core/test/fake_web_history_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/version_info/version_info.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {

class HistoryNoticeUtilsTest : public ::testing::Test {
 public:
  HistoryNoticeUtilsTest() {}

  void SetUp() override {
    sync_service_ = std::make_unique<syncer::TestSyncService>();
    history_service_ = std::make_unique<history::FakeWebHistoryService>();
    history_service_->SetupFakeResponse(true /* success */, net::HTTP_OK);
  }

  syncer::TestSyncService* sync_service() { return sync_service_.get(); }

  history::FakeWebHistoryService* history_service() {
    return history_service_.get();
  }

  void ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(
      bool expected_test_case_result) {
    bool result;
    base::RunLoop run_loop;
    ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
        sync_service_.get(), history_service_.get(),
        version_info::Channel::STABLE, base::BindLambdaForTesting([&](bool r) {
          result = r;
          run_loop.Quit();
        }));
    run_loop.Run();

    // Process the DeleteSoon() called on MergeBooleanCallbacks, otherwise
    // this it will be considered to be leaked.
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(expected_test_case_result, result);
  }

 private:
  scoped_refptr<net::URLRequestContextGetter> url_request_context_;
  std::unique_ptr<syncer::TestSyncService> sync_service_;
  std::unique_ptr<history::FakeWebHistoryService> history_service_;

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(HistoryNoticeUtilsTest, NotSyncing) {
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(false);
}

TEST_F(HistoryNoticeUtilsTest, SyncingWithWrongParameters) {
  // Regardless of the state of the web history...
  history_service()->SetWebAndAppActivityEnabled(true);
  history_service()->SetOtherFormsOfBrowsingHistoryPresent(true);

  // ...the response is false if there's custom passphrase...
  sync_service()->SetActiveDataTypes(syncer::ModelTypeSet::All());
  sync_service()->SetIsUsingSecondaryPassphrase(true);
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(false);

  // ...or even if there's no custom passphrase, but we're not syncing history.
  syncer::ModelTypeSet only_passwords(syncer::PASSWORDS);
  sync_service()->SetActiveDataTypes(only_passwords);
  sync_service()->SetIsUsingSecondaryPassphrase(false);
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(false);
}

TEST_F(HistoryNoticeUtilsTest, WebHistoryStates) {
  // If history Sync is active...
  sync_service()->SetActiveDataTypes(syncer::ModelTypeSet::All());

  // ...the result is true if both web history queries return true...
  history_service()->SetWebAndAppActivityEnabled(true);
  history_service()->SetOtherFormsOfBrowsingHistoryPresent(true);
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(true);

  // ...but not otherwise.
  history_service()->SetOtherFormsOfBrowsingHistoryPresent(false);
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(false);
  history_service()->SetWebAndAppActivityEnabled(false);
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(false);
  history_service()->SetOtherFormsOfBrowsingHistoryPresent(true);
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(false);

  // Invalid responses from the web history are interpreted as false.
  history_service()->SetWebAndAppActivityEnabled(true);
  history_service()->SetOtherFormsOfBrowsingHistoryPresent(true);
  history_service()->SetupFakeResponse(true, net::HTTP_INTERNAL_SERVER_ERROR);
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(false);
  history_service()->SetupFakeResponse(false, net::HTTP_OK);
  ExpectShouldPopupDialogAboutOtherFormsOfBrowsingHistoryWithResult(false);
}

}  // namespace browsing_data
