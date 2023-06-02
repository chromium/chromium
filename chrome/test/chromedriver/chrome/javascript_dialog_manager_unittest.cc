// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/recorder_devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(JavaScriptDialogManager, NoDialog) {
  StubDevToolsClient client;
  JavaScriptDialogManager manager(&client);
  std::string message("HI");
  ASSERT_EQ(kNoSuchAlert, manager.GetDialogMessage(&message).code());
  ASSERT_FALSE(manager.IsDialogOpen());
  ASSERT_STREQ("HI", message.c_str());
  ASSERT_EQ(kNoSuchAlert, manager.HandleDialog(false, nullptr).code());
}

TEST(JavaScriptDialogManager, HandleDialogPassesParams) {
  RecorderDevToolsClient client;
  JavaScriptDialogManager manager(&client);
  base::Value::Dict params;
  params.Set("message", "hi");
  params.Set("type", "prompt");
  params.Set("defaultPrompt", "This is a default text");
  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.javascriptDialogOpening", params).code());
  std::string given_text("text");
  ASSERT_EQ(kOk, manager.HandleDialog(false, &given_text).code());
  const std::string* text = client.commands_[0].params.FindString("promptText");
  ASSERT_TRUE(text);
  ASSERT_EQ(given_text, *text);
  ASSERT_TRUE(client.commands_[0].params.contains("accept"));
}

TEST(JavaScriptDialogManager, HandleDialogNullPrompt) {
  RecorderDevToolsClient client;
  JavaScriptDialogManager manager(&client);
  base::Value::Dict params;
  params.Set("message", "hi");
  params.Set("type", "prompt");
  params.Set("defaultPrompt", "");
  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.javascriptDialogOpening", params).code());
  ASSERT_EQ(kOk, manager.HandleDialog(false, nullptr).code());
  ASSERT_TRUE(client.commands_[0].params.contains("promptText"));
  ASSERT_TRUE(client.commands_[0].params.contains("accept"));
}

TEST(JavaScriptDialogManager, ReconnectClearsStateAndSendsEnable) {
  RecorderDevToolsClient client;
  JavaScriptDialogManager manager(&client);
  base::Value::Dict params;
  params.Set("message", "hi");
  params.Set("type", "alert");
  params.Set("defaultPrompt", "");
  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.javascriptDialogOpening", params).code());
  ASSERT_TRUE(manager.IsDialogOpen());
  std::string message;
  ASSERT_EQ(kOk, manager.GetDialogMessage(&message).code());

  ASSERT_TRUE(manager.OnConnected(&client).IsOk());
  ASSERT_EQ("Page.enable", client.commands_[0].method);
  ASSERT_FALSE(manager.IsDialogOpen());
  ASSERT_EQ(kNoSuchAlert, manager.GetDialogMessage(&message).code());
  ASSERT_EQ(kNoSuchAlert, manager.HandleDialog(false, nullptr).code());
}

namespace {

class FakeDevToolsClient : public StubDevToolsClient {
 public:
  FakeDevToolsClient() = default;
  ~FakeDevToolsClient() override = default;

  void set_closing_count(int closing_count) {
    closing_count_ = closing_count;
  }

  // Overridden from StubDevToolsClient:
  Status SendCommandAndGetResult(const std::string& method,
                                 const base::Value::Dict& params,
                                 base::Value::Dict* result) override {
    while (closing_count_ > 0) {
      Status status = listener_->OnEvent(this, "Page.javascriptDialogClosed",
                                         base::Value::Dict());
      if (status.IsError())
        return status;
      closing_count_--;
    }
    return Status(kOk);
  }

  void AddListener(DevToolsEventListener* listener) override {
    listener_ = listener;
  }

  void RemoveListener(DevToolsEventListener* listener) override {
    if (listener == listener_) {
      listener_ = nullptr;
    }
  }

 private:
  raw_ptr<DevToolsEventListener> listener_ = nullptr;
  int closing_count_ = 0;
};

}  // namespace

TEST(JavaScriptDialogManager, OneDialog) {
  FakeDevToolsClient client;
  JavaScriptDialogManager manager(&client);
  base::Value::Dict params;
  params.Set("message", "hi");
  params.Set("type", "alert");
  params.Set("defaultPrompt", "");
  ASSERT_FALSE(manager.IsDialogOpen());
  std::string message;
  ASSERT_EQ(kNoSuchAlert, manager.GetDialogMessage(&message).code());

  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.javascriptDialogOpening", params).code());
  ASSERT_TRUE(manager.IsDialogOpen());
  ASSERT_EQ(kOk, manager.GetDialogMessage(&message).code());
  ASSERT_EQ("hi", message);
  std::string type;
  ASSERT_EQ(kOk, manager.GetTypeOfDialog(&type).code());
  ASSERT_EQ("alert", type);

  client.set_closing_count(1);
  ASSERT_EQ(kOk, manager.HandleDialog(false, nullptr).code());
  ASSERT_FALSE(manager.IsDialogOpen());
  ASSERT_EQ(kNoSuchAlert, manager.GetDialogMessage(&message).code());
  ASSERT_EQ(kNoSuchAlert, manager.HandleDialog(false, nullptr).code());
}

TEST(JavaScriptDialogManager, TwoDialogs) {
  FakeDevToolsClient client;
  JavaScriptDialogManager manager(&client);
  base::Value::Dict params;
  params.Set("message", "1");
  params.Set("type", "confirm");
  params.Set("defaultPrompt", "");
  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.javascriptDialogOpening", params).code());
  params.Set("message", "2");
  params.Set("type", "alert");
  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.javascriptDialogOpening", params).code());

  std::string message;
  std::string type;
  ASSERT_EQ(kOk, manager.GetDialogMessage(&message).code());
  ASSERT_EQ(kOk, manager.GetTypeOfDialog(&type).code());
  ASSERT_TRUE(manager.IsDialogOpen());
  ASSERT_EQ("1", message);
  ASSERT_EQ("confirm", type);

  ASSERT_EQ(kOk, manager.HandleDialog(false, nullptr).code());
  ASSERT_TRUE(manager.IsDialogOpen());
  ASSERT_EQ(kOk, manager.GetDialogMessage(&message).code());
  ASSERT_EQ(kOk, manager.GetTypeOfDialog(&type).code());
  ASSERT_EQ("2", message);
  ASSERT_EQ("alert", type);

  client.set_closing_count(2);
  ASSERT_EQ(kOk, manager.HandleDialog(false, nullptr).code());
  ASSERT_FALSE(manager.IsDialogOpen());
  ASSERT_EQ(kNoSuchAlert, manager.GetDialogMessage(&message).code());
  ASSERT_EQ(kNoSuchAlert, manager.HandleDialog(false, nullptr).code());
}

TEST(JavaScriptDialogManager, OneDialogManualClose) {
  StubDevToolsClient client;
  BrowserInfo browser_info;
  JavaScriptDialogManager manager(&client);
  base::Value::Dict params;
  params.Set("message", "hi");
  params.Set("type", "alert");
  params.Set("defaultPrompt", "");
  ASSERT_FALSE(manager.IsDialogOpen());
  std::string message;
  ASSERT_EQ(kNoSuchAlert, manager.GetDialogMessage(&message).code());

  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.javascriptDialogOpening", params).code());
  ASSERT_TRUE(manager.IsDialogOpen());
  ASSERT_EQ(kOk, manager.GetDialogMessage(&message).code());
  ASSERT_EQ("hi", message);
  std::string type;
  ASSERT_EQ(kOk, manager.GetTypeOfDialog(&type).code());
  ASSERT_EQ("alert", type);

  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.javascriptDialogClosed", params).code());
  ASSERT_FALSE(manager.IsDialogOpen());
  ASSERT_EQ(kNoSuchAlert, manager.GetDialogMessage(&message).code());
  ASSERT_EQ(kNoSuchAlert, manager.HandleDialog(false, nullptr).code());
}
