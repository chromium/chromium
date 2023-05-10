// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/fedcm_tracker.h"

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace {

class MockDevToolsClient : public StubDevToolsClient {
 public:
  MOCK_METHOD2(SendCommand,
               Status(const std::string& method,
                      const base::Value::Dict& params));
  MOCK_METHOD1(AddListener, void(DevToolsEventListener* listener));
};

}  // namespace

class FedCmTrackerTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    EXPECT_CALL(devtools_client_, AddListener(_));
    EXPECT_CALL(devtools_client_, SendCommand("FedCm.enable", _))
        .WillOnce(Return(Status(kOk)));
    fedcm_tracker_ = std::make_unique<FedCmTracker>(&devtools_client_);
    Status status = fedcm_tracker_->Enable(&devtools_client_);
    EXPECT_TRUE(status.IsOk());
  }

 protected:
  std::unique_ptr<FedCmTracker> fedcm_tracker_;
  MockDevToolsClient devtools_client_;
};

TEST_F(FedCmTrackerTest, OnDialogShown) {
  base::Value::Dict account;
  account.Set("accountId", "1");
  account.Set("email", "foo@bar.com");
  account.Set("name", "Foo Bar");
  base::Value::List account_list;
  account_list.Append(std::move(account));
  base::Value::Dict event_params;
  event_params.Set("dialogId", "5");
  event_params.Set("title", "a title");
  event_params.Set("dialogType", "AccountChooser");
  event_params.Set("accounts", std::move(account_list));

  EXPECT_FALSE(fedcm_tracker_->HasDialog());

  fedcm_tracker_->OnEvent(&devtools_client_, "FedCm.dialogShown", event_params);
  EXPECT_TRUE(fedcm_tracker_->HasDialog());
  EXPECT_EQ("5", fedcm_tracker_->GetLastDialogId());
  EXPECT_EQ("a title", fedcm_tracker_->GetLastTitle());
  EXPECT_EQ(1u, fedcm_tracker_->GetLastAccounts().size());
  EXPECT_EQ("AccountChooser", fedcm_tracker_->GetLastDialogType());
}
