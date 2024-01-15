// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/chrome/stub_web_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::Eq;
using testing::Pointee;

testing::AssertionResult StatusOk(const Status& status) {
  if (status.IsOk()) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure() << status.message();
  }
}

void SaveTo(std::string* dest, std::string value) {
  *dest = std::move(value);
}

class MockChrome : public StubChrome {
 public:
  MockChrome() : web_view_("1") {}
  ~MockChrome() override {}

  Status GetWebViewById(const std::string& id, WebView** web_view) override {
    if (id == web_view_.GetId()) {
      *web_view = &web_view_;
      return Status(kOk);
    }
    return Status(kUnknownError);
  }

 private:
  StubWebView web_view_;
};

}  // namespace

TEST(Session, GetTargetWindowNoChrome) {
  Session session("1");
  WebView* web_view;
  ASSERT_EQ(kNoSuchWindow, session.GetTargetWindow(&web_view).code());
}

TEST(Session, GetTargetWindowTargetWindowClosed) {
  std::unique_ptr<Chrome> chrome(new MockChrome());
  Session session("1", std::move(chrome));
  session.window = "2";
  WebView* web_view;
  ASSERT_EQ(kNoSuchWindow, session.GetTargetWindow(&web_view).code());
}

TEST(Session, GetTargetWindowTargetWindowStillOpen) {
  std::unique_ptr<Chrome> chrome(new MockChrome());
  Session session("1", std::move(chrome));
  session.window = "1";
  WebView* web_view = nullptr;
  ASSERT_EQ(kOk, session.GetTargetWindow(&web_view).code());
  ASSERT_TRUE(web_view);
}

TEST(Session, SwitchToParentFrame) {
  std::unique_ptr<Chrome> chrome(new MockChrome());
  Session session("1", std::move(chrome));

  // Initial frame should be top frame.
  ASSERT_EQ(std::string(), session.GetCurrentFrameId());

  // Switching to parent frame should be a no-op.
  session.SwitchToParentFrame();
  ASSERT_EQ(std::string(), session.GetCurrentFrameId());

  session.SwitchToSubFrame("1.1", std::string());
  ASSERT_EQ("1.1", session.GetCurrentFrameId());
  session.SwitchToParentFrame();
  ASSERT_EQ(std::string(), session.GetCurrentFrameId());

  session.SwitchToSubFrame("2.1", std::string());
  ASSERT_EQ("2.1", session.GetCurrentFrameId());
  session.SwitchToSubFrame("2.2", std::string());
  ASSERT_EQ("2.2", session.GetCurrentFrameId());
  session.SwitchToParentFrame();
  ASSERT_EQ("2.1", session.GetCurrentFrameId());
  session.SwitchToParentFrame();
  ASSERT_EQ(std::string(), session.GetCurrentFrameId());
}

TEST(Session, SwitchToTopFrame) {
  std::unique_ptr<Chrome> chrome(new MockChrome());
  Session session("1", std::move(chrome));

  // Initial frame should be top frame.
  ASSERT_EQ(std::string(), session.GetCurrentFrameId());

  // Switching to top frame should be a no-op.
  session.SwitchToTopFrame();
  ASSERT_EQ(std::string(), session.GetCurrentFrameId());

  session.SwitchToSubFrame("3.1", std::string());
  ASSERT_EQ("3.1", session.GetCurrentFrameId());
  session.SwitchToSubFrame("3.2", std::string());
  ASSERT_EQ("3.2", session.GetCurrentFrameId());
  session.SwitchToTopFrame();
  ASSERT_EQ(std::string(), session.GetCurrentFrameId());
}

TEST(Session, SplitChannel1) {
  std::string channel = "/uno/2/tres";
  int connection_id = -15;
  std::string suffix;
  EXPECT_TRUE(
      StatusOk(internal::SplitChannel(&channel, &connection_id, &suffix)));
  EXPECT_EQ("/uno", channel);
  EXPECT_EQ(2, connection_id);
  EXPECT_EQ("/tres", suffix);
}

TEST(Session, SplitChannel2) {
  std::string channel = "/1/dos";
  int connection_id = -15;
  std::string suffix;
  EXPECT_TRUE(
      StatusOk(internal::SplitChannel(&channel, &connection_id, &suffix)));
  EXPECT_EQ("", channel);
  EXPECT_EQ(1, connection_id);
  EXPECT_EQ("/dos", suffix);
}

TEST(Session, SplitChannelNonIntegerConnection) {
  std::string channel = "/uno/dos/tres";
  int connection_id = -15;
  std::string suffix;
  EXPECT_TRUE(
      internal::SplitChannel(&channel, &connection_id, &suffix).IsError());
}

TEST(Session, SplitChannelNoSeparators) {
  std::string channel = "no-separators";
  int connection_id = -15;
  std::string suffix;
  EXPECT_TRUE(
      internal::SplitChannel(&channel, &connection_id, &suffix).IsError());
}

TEST(Session, SplitChannelEmpty) {
  std::string channel = "";
  int connection_id = -15;
  std::string suffix;
  EXPECT_TRUE(
      internal::SplitChannel(&channel, &connection_id, &suffix).IsError());
}

TEST(Session, OnBidiResponseChan) {
  std::unique_ptr<Chrome> chrome(new MockChrome());
  Session session("1", std::move(chrome));
  std::string received;
  session.AddBidiConnection(512, base::BindRepeating(&SaveTo, &received),
                            base::BindRepeating([] {}));
  base::Value::Dict payload;
  payload.Set("channel", std::string("abc/512") + Session::kChannelSuffix);
  payload.Set("data", "ok");
  EXPECT_TRUE(StatusOk(session.OnBidiResponse(std::move(payload))));
  std::optional<base::Value> data_parsed =
      base::JSONReader::Read(received, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(data_parsed);
  ASSERT_TRUE(data_parsed->is_dict());
  EXPECT_THAT(data_parsed->GetDict().FindString("channel"), Pointee(Eq("abc")));
  EXPECT_THAT(data_parsed->GetDict().FindString("data"), Pointee(Eq("ok")));
}

TEST(Session, OnBidiResponseNoChan) {
  std::unique_ptr<Chrome> chrome(new MockChrome());
  Session session("1", std::move(chrome));
  std::string received;
  session.AddBidiConnection(512, base::BindRepeating(&SaveTo, &received),
                            base::BindRepeating([] {}));
  base::Value::Dict payload;
  payload.Set("channel", std::string("/512") + Session::kNoChannelSuffix);
  payload.Set("data", "ok");
  EXPECT_TRUE(StatusOk(session.OnBidiResponse(std::move(payload))));
  std::optional<base::Value> data_parsed =
      base::JSONReader::Read(received, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(data_parsed);
  ASSERT_TRUE(data_parsed->is_dict());
  EXPECT_EQ(nullptr, data_parsed->GetDict().FindString("channel"));
  EXPECT_THAT(data_parsed->GetDict().FindString("data"), Pointee(Eq("ok")));
}

TEST(Session, OnBidiResponseNullChan) {
  std::unique_ptr<Chrome> chrome(new MockChrome());
  Session session("1", std::move(chrome));
  std::string received;
  session.AddBidiConnection(512, base::BindRepeating(&SaveTo, &received),
                            base::BindRepeating([] {}));
  base::Value::Dict payload;
  payload.Set("data", "ok");
  EXPECT_TRUE(session.OnBidiResponse(std::move(payload)).IsError());
  EXPECT_EQ("", received);
}

TEST(Session, OnBidiResponseUnexpectedChannel1) {
  std::unique_ptr<Chrome> chrome(new MockChrome());
  Session session("1", std::move(chrome));
  std::string received;
  session.AddBidiConnection(512, base::BindRepeating(&SaveTo, &received),
                            base::BindRepeating([] {}));
  base::Value::Dict payload;
  payload.Set("channel", "x/512/unexpected");
  payload.Set("data", "ok");
  EXPECT_TRUE(session.OnBidiResponse(std::move(payload)).IsError());
  EXPECT_EQ("", received);
}

TEST(Session, OnBidiResponseUnexpectedChannel2) {
  std::unique_ptr<Chrome> chrome(new MockChrome());
  Session session("1", std::move(chrome));
  std::string received;
  session.AddBidiConnection(512, base::BindRepeating(&SaveTo, &received),
                            base::BindRepeating([] {}));
  base::Value::Dict payload;
  payload.Set("channel", "unexpected");
  payload.Set("data", "ok");
  EXPECT_TRUE(session.OnBidiResponse(std::move(payload)).IsError());
  EXPECT_EQ("", received);
}

TEST(Session, OnBidiResponseUnknownConnection) {
  std::unique_ptr<Chrome> chrome(new MockChrome());
  Session session("1", std::move(chrome));
  std::string received;
  session.AddBidiConnection(136, base::BindRepeating(&SaveTo, &received),
                            base::BindRepeating([] {}));
  base::Value::Dict payload;
  payload.Set("channel", std::string("/5") + Session::kNoChannelSuffix);
  payload.Set("data", "ok");
  // Response must be accepted as it is addressed to a closed connection.
  // However no connection should actually receive it
  EXPECT_TRUE(StatusOk(session.OnBidiResponse(std::move(payload))));
  EXPECT_EQ("", received);
}

TEST(Session, OnBidiResponseRemovedConnection) {
  std::unique_ptr<Chrome> chrome(new MockChrome());
  Session session("1", std::move(chrome));
  std::string received1;
  std::string received2;
  session.AddBidiConnection(1, base::BindRepeating(&SaveTo, &received1),
                            base::BindRepeating([] {}));
  session.AddBidiConnection(2, base::BindRepeating(&SaveTo, &received2),
                            base::BindRepeating([] {}));
  session.RemoveBidiConnection(1);
  base::Value::Dict payload;
  payload.Set("channel", std::string("/1") + Session::kNoChannelSuffix);
  payload.Set("data", "ok");
  // Response must be accepted as it is addressed to a closed connection.
  // However no connection should actually receive it
  EXPECT_TRUE(StatusOk(session.OnBidiResponse(std::move(payload))));
  EXPECT_EQ("", received1);
  EXPECT_EQ("", received2);
}

TEST(Session, OnBidiResponseAfterCloseAllConnections) {
  std::unique_ptr<Chrome> chrome(new MockChrome());
  Session session("1", std::move(chrome));
  std::string received;
  session.AddBidiConnection(5, base::BindRepeating(&SaveTo, &received),
                            base::BindRepeating([] {}));
  session.CloseAllConnections();
  base::Value::Dict payload;
  payload.Set("channel", std::string("/5") + Session::kNoChannelSuffix);
  payload.Set("data", "ok");
  // Response must be accepted as it is addressed to a closed connection.
  // However no connection should actually receive it
  EXPECT_TRUE(StatusOk(session.OnBidiResponse(std::move(payload))));
  EXPECT_EQ("", received);
}

TEST(Session, OnBidiResponseCorrectConnection) {
  std::unique_ptr<Chrome> chrome(new MockChrome());
  Session session("1", std::move(chrome));
  std::string received1;
  std::string received2;
  std::string received3;
  session.AddBidiConnection(1, base::BindRepeating(&SaveTo, &received1),
                            base::BindRepeating([] {}));
  session.AddBidiConnection(2, base::BindRepeating(&SaveTo, &received2),
                            base::BindRepeating([] {}));
  session.AddBidiConnection(3, base::BindRepeating(&SaveTo, &received3),
                            base::BindRepeating([] {}));
  base::Value::Dict payload;
  payload.Set("channel", std::string("abc/2") + Session::kChannelSuffix);
  payload.Set("data", "ok");
  EXPECT_TRUE(StatusOk(session.OnBidiResponse(std::move(payload))));
  EXPECT_EQ("", received1);
  EXPECT_EQ("", received3);
  std::optional<base::Value> data_parsed =
      base::JSONReader::Read(received2, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(data_parsed);
  ASSERT_TRUE(data_parsed->is_dict());
  EXPECT_THAT(data_parsed->GetDict().FindString("channel"), Pointee(Eq("abc")));
  EXPECT_THAT(data_parsed->GetDict().FindString("data"), Pointee(Eq("ok")));
}

TEST(Session, OnBidiResponseFormat) {
  std::unique_ptr<Chrome> chrome(new MockChrome());
  Session session("1", std::move(chrome));
  std::string received;
  session.AddBidiConnection(512, base::BindRepeating(&SaveTo, &received),
                            base::BindRepeating([] {}));
  base::Value::Dict payload;
  payload.Set("channel", std::string("abc/512") + Session::kChannelSuffix);
  payload.Set("string_field", "some_String");
  payload.Set("integer_field", 1);
  payload.Set("float_field", 1.234);
  EXPECT_TRUE(StatusOk(session.OnBidiResponse(std::move(payload))));
  EXPECT_EQ(
      "{\"channel\":\"abc\",\"float_field\":1.234,\"integer_field\":1,\"string_"
      "field\":\"some_String\"}",
      received);
}

TEST(Session, OnBlockingChannelResponseWhileAwaiting) {
  std::string good_blocking_channels[] = {
      std::string("x/7") + Session::kChannelSuffix +
          Session::kBlockingChannelSuffix,
      std::string("/7") + Session::kNoChannelSuffix +
          Session::kBlockingChannelSuffix,
  };
  for (std::string channel : good_blocking_channels) {
    std::unique_ptr<Chrome> chrome(new MockChrome());
    Session session("1", std::move(chrome));
    session.awaiting_bidi_response = true;
    session.AddBidiConnection(7, base::BindRepeating([](std::string) {}),
                              base::BindRepeating([] {}));
    base::Value::Dict payload;
    payload.Set("channel", channel);
    EXPECT_TRUE(StatusOk(session.OnBidiResponse(std::move(payload))));
    EXPECT_FALSE(session.awaiting_bidi_response);
  }
}

TEST(Session, OnNonBlockingChannelResponseWhileAwaiting) {
  std::string good_non_blocking_channels[] = {
      std::string("x/7") + Session::kChannelSuffix,
      std::string("/7") + Session::kNoChannelSuffix};
  for (std::string channel : good_non_blocking_channels) {
    std::unique_ptr<Chrome> chrome(new MockChrome());
    Session session("1", std::move(chrome));
    session.awaiting_bidi_response = true;
    session.AddBidiConnection(7, base::BindRepeating([](std::string) {}),
                              base::BindRepeating([] {}));
    base::Value::Dict payload;
    payload.Set("channel", channel);
    EXPECT_TRUE(StatusOk(session.OnBidiResponse(std::move(payload))));
    EXPECT_TRUE(session.awaiting_bidi_response);
  }
}

TEST(Session, OnBlockingChannelResponseWhileNotAwaiting) {
  std::string good_blocking_channels[] = {
      std::string("x/7") + Session::kChannelSuffix +
          Session::kBlockingChannelSuffix,
      std::string("/7") + Session::kNoChannelSuffix +
          Session::kBlockingChannelSuffix,
  };
  for (std::string channel : good_blocking_channels) {
    std::unique_ptr<Chrome> chrome(new MockChrome());
    Session session("1", std::move(chrome));
    session.awaiting_bidi_response = false;
    session.AddBidiConnection(7, base::BindRepeating([](std::string) {}),
                              base::BindRepeating([] {}));
    base::Value::Dict payload;
    payload.Set("channel", channel);
    EXPECT_TRUE(session.OnBidiResponse(std::move(payload)).IsError());
    EXPECT_FALSE(session.awaiting_bidi_response);
  }
}
