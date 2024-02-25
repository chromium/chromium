// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/fedcm_commands.h"

#include "chrome/test/chromedriver/chrome/fedcm_tracker.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "chrome/test/chromedriver/chrome/stub_web_view.h"
#include "chrome/test/chromedriver/session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockResponseWebView : public StubWebView {
 public:
  explicit MockResponseWebView(DevToolsClient* client)
      : StubWebView("1"), tracker_(client) {}
  ~MockResponseWebView() override = default;

  Status SendCommandAndGetResult(
      const std::string& command,
      const base::Value::Dict& params,
      std::unique_ptr<base::Value>* result) override {
    last_command_ = command;
    return Status(kOk);
  }

  Status GetFedCmTracker(FedCmTracker** out_tracker) override {
    *out_tracker = &tracker_;
    return Status(kOk);
  }

  void SendEvent(DevToolsClient* client) {
    base::Value::Dict dict;
    dict.Set("dialogId", "0");
    dict.Set("title", "Title");
    dict.Set("dialogType", "AccountChooser");
    base::Value::Dict account;
    account.Set("accountId", "123");
    account.Set("email", "foo@bar.com");
    account.Set("name", "Foo Bar");
    account.Set("givenName", "Foo");
    account.Set("pictureUrl", "https://pics/pic.jpg");
    account.Set("idpConfigUrl", "https://idp.example/fedcm.json");
    account.Set("idpLoginUrl", "https://idp.example/login");
    account.Set("loginState", "SignIn");

    base::Value::List accounts;
    accounts.Append(std::move(account));
    dict.Set("accounts", std::move(accounts));

    tracker_.OnEvent(client, "FedCm.dialogShown", dict);
  }

  const std::string& GetLastCommand() const { return last_command_; }

  bool IsDetached() const override { return false; }

  Status CallFunctionWithTimeout(
      const std::string& frame,
      const std::string& function,
      const base::Value::List& args,
      const base::TimeDelta& timeout,
      std::unique_ptr<base::Value>* result) override {
    return Status{kOk};
  }

 private:
  FedCmTracker tracker_;
  std::string last_command_;
};

class FedCmCommandsTest : public testing::Test {
 public:
  FedCmCommandsTest() = default;

 protected:
  Session session{"id"};
  StubDevToolsClient client;
  MockResponseWebView web_view{&client};
};

}  // namespace

TEST_F(FedCmCommandsTest, ExecuteCancelDialog) {
  base::Value::Dict params;
  std::unique_ptr<base::Value> value;

  Status status =
      ExecuteCancelDialog(&session, &web_view, params, &value, nullptr);
  EXPECT_EQ(kNoSuchAlert, status.code());

  web_view.SendEvent(&client);

  status = ExecuteCancelDialog(&session, &web_view, params, &value, nullptr);
  EXPECT_EQ(kOk, status.code());

  EXPECT_EQ("FedCm.dismissDialog", web_view.GetLastCommand());
}

TEST_F(FedCmCommandsTest, ExecuteSelectAccount) {
  base::Value::Dict params;
  std::unique_ptr<base::Value> value;

  Status status =
      ExecuteSelectAccount(&session, &web_view, params, &value, nullptr);
  EXPECT_EQ(kNoSuchAlert, status.code());

  web_view.SendEvent(&client);

  // No account index
  status = ExecuteSelectAccount(&session, &web_view, params, &value, nullptr);
  EXPECT_EQ(kInvalidArgument, status.code());

  params.Set("accountIndex", 0);
  status = ExecuteSelectAccount(&session, &web_view, params, &value, nullptr);
  EXPECT_EQ(kOk, status.code());

  EXPECT_EQ("FedCm.selectAccount", web_view.GetLastCommand());
}

TEST_F(FedCmCommandsTest, ExecuteGetAccounts) {
  base::Value::Dict params;
  std::unique_ptr<base::Value> value;

  Status status =
      ExecuteGetAccounts(&session, &web_view, params, &value, nullptr);
  ASSERT_EQ(kNoSuchAlert, status.code());

  web_view.SendEvent(&client);

  status = ExecuteGetAccounts(&session, &web_view, params, &value, nullptr);
  ASSERT_EQ(kOk, status.code());
  base::Value::List* response = value->GetIfList();
  ASSERT_TRUE(response);
  ASSERT_EQ(1u, response->size());
  base::Value::Dict* account = response->front().GetIfDict();
  ASSERT_TRUE(account);
  std::string* accountId = account->FindString("accountId");
  ASSERT_TRUE(accountId);
  EXPECT_EQ(*accountId, "123");
  std::string* email = account->FindString("email");
  ASSERT_TRUE(email);
  EXPECT_EQ(*email, "foo@bar.com");
  std::string* name = account->FindString("name");
  ASSERT_TRUE(name);
  EXPECT_EQ(*name, "Foo Bar");
  std::string* givenName = account->FindString("givenName");
  ASSERT_TRUE(givenName);
  EXPECT_EQ(*givenName, "Foo");
  std::string* pictureUrl = account->FindString("pictureUrl");
  ASSERT_TRUE(pictureUrl);
  EXPECT_EQ(*pictureUrl, "https://pics/pic.jpg");
  std::string* idpConfigUrl = account->FindString("idpConfigUrl");
  ASSERT_TRUE(idpConfigUrl);
  EXPECT_EQ(*idpConfigUrl, "https://idp.example/fedcm.json");
  std::string* idpLoginUrl = account->FindString("idpLoginUrl");
  ASSERT_TRUE(idpLoginUrl);
  EXPECT_EQ(*idpLoginUrl, "https://idp.example/login");
  std::string* loginState = account->FindString("loginState");
  ASSERT_TRUE(loginState);
  EXPECT_EQ(*loginState, "SignIn");

  // This should not have triggered any commands.
  EXPECT_EQ("", web_view.GetLastCommand());
}

TEST_F(FedCmCommandsTest, ExecuteGetTitle) {
  base::Value::Dict params;
  std::unique_ptr<base::Value> value;

  Status status =
      ExecuteGetFedCmTitle(&session, &web_view, params, &value, nullptr);
  ASSERT_EQ(kNoSuchAlert, status.code());

  web_view.SendEvent(&client);

  status = ExecuteGetFedCmTitle(&session, &web_view, params, &value, nullptr);
  ASSERT_EQ(kOk, status.code());
  base::Value::Dict* dict = value->GetIfDict();
  ASSERT_TRUE(dict);
  std::string* title = dict->FindString("title");
  ASSERT_TRUE(title);
  EXPECT_EQ(*title, "Title");

  // This should not have triggered any commands.
  EXPECT_EQ("", web_view.GetLastCommand());
}

TEST_F(FedCmCommandsTest, ExecuteGetDialogType) {
  base::Value::Dict params;
  std::unique_ptr<base::Value> value;

  Status status =
      ExecuteGetDialogType(&session, &web_view, params, &value, nullptr);
  ASSERT_EQ(kNoSuchAlert, status.code());

  web_view.SendEvent(&client);

  status = ExecuteGetDialogType(&session, &web_view, params, &value, nullptr);
  ASSERT_EQ(kOk, status.code());
  std::string* type = value->GetIfString();
  ASSERT_TRUE(type);
  EXPECT_EQ(*type, "AccountChooser");

  // This should not have triggered any commands.
  EXPECT_EQ("", web_view.GetLastCommand());
}

TEST_F(FedCmCommandsTest, ExecuteSetDelayEnabled) {
  base::Value::Dict params;
  std::unique_ptr<base::Value> value;

  // No enabled argument.
  Status status =
      ExecuteSetDelayEnabled(&session, &web_view, params, &value, nullptr);
  EXPECT_EQ(kInvalidArgument, status.code());

  params.Set("enabled", false);
  status = ExecuteSetDelayEnabled(&session, &web_view, params, &value, nullptr);
  EXPECT_EQ(kOk, status.code());

  EXPECT_EQ("FedCm.enable", web_view.GetLastCommand());
}

TEST_F(FedCmCommandsTest, ExecuteResetCooldown) {
  base::Value::Dict params;
  std::unique_ptr<base::Value> value;

  Status status =
      ExecuteResetCooldown(&session, &web_view, params, &value, nullptr);
  EXPECT_EQ(kOk, status.code());

  EXPECT_EQ("FedCm.resetCooldown", web_view.GetLastCommand());
}
