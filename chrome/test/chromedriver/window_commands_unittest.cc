// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/window_commands.h"

#include <memory>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "base/types/optional_util.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/mobile_emulation_override_manager.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "chrome/test/chromedriver/chrome/stub_web_view.h"
#include "chrome/test/chromedriver/commands.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockChrome : public StubChrome {
 public:
  MockChrome() : web_view_("1") {}
  ~MockChrome() override = default;

  Status GetWebViewById(const std::string& id, WebView** web_view) override {
    if (id == web_view_.GetId()) {
      *web_view = &web_view_;
      return Status(kOk);
    }
    return Status(kUnknownError);
  }

 private:
  // Using a StubWebView does not allow testing the functionality end-to-end,
  // more details in crbug.com/40579857
  StubWebView web_view_;
};

typedef Status (*Command)(Session* session,
                          WebView* web_view,
                          const base::DictValue& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout);

Status CallWindowCommand(Command command,
                         const base::DictValue& params = {},
                         std::unique_ptr<base::Value>* value = nullptr) {
  MockChrome* chrome = new MockChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));
  WebView* web_view = nullptr;
  Status status = chrome->GetWebViewById("1", &web_view);
  if (status.IsError())
    return status;

  std::unique_ptr<base::Value> local_value;
  Timeout timeout;
  return command(&session, web_view, params, value ? value : &local_value,
                 &timeout);
}

Status CallWindowCommand(Command command,
                         StubWebView* web_view,
                         const base::DictValue& params = {},
                         std::unique_ptr<base::Value>* value = nullptr) {
  MockChrome* chrome = new MockChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));

  std::unique_ptr<base::Value> local_value;
  Timeout timeout;
  return command(&session, web_view, params, value ? value : &local_value,
                 &timeout);
}

}  // namespace

TEST(WindowCommandsTest, ExecuteFreeze) {
  Status status = CallWindowCommand(ExecuteFreeze);
  ASSERT_EQ(kOk, status.code());
}

TEST(WindowCommandsTest, ExecuteResume) {
  Status status = CallWindowCommand(ExecuteResume);
  ASSERT_EQ(kOk, status.code());
}

TEST(WindowCommandsTest, ExecuteSendCommandAndGetResult_NoCmd) {
  base::DictValue params;
  params.Set("params", base::DictValue());
  Status status = CallWindowCommand(ExecuteSendCommandAndGetResult, params);
  ASSERT_EQ(kInvalidArgument, status.code());
  ASSERT_NE(status.message().find("command not passed"), std::string::npos);
}

TEST(WindowCommandsTest, ExecuteSendCommandAndGetResult_NoParams) {
  base::DictValue params;
  params.Set("cmd", "CSS.enable");
  Status status = CallWindowCommand(ExecuteSendCommandAndGetResult, params);
  ASSERT_EQ(kInvalidArgument, status.code());
  ASSERT_NE(status.message().find("params not passed"), std::string::npos);
}

TEST(WindowCommandsTest, ProcessInputActionSequencePointerMouse) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action_sequence;
  base::ListValue actions;
  base::DictValue parameters;
  parameters.Set("pointerType", "mouse");
  action_sequence.Set("parameters", std::move(parameters));
  {
    base::DictValue action;
    action.Set("type", "pointerMove");
    action.Set("x", 30);
    action.Set("y", 60);
    actions.Append(std::move(action));
  }
  {
    base::DictValue action;
    action.Set("type", "pointerDown");
    action.Set("button", 0);
    actions.Append(std::move(action));
  }
  {
    base::DictValue action;
    action.Set("type", "pointerUp");
    action.Set("button", 0);
    actions.Append(std::move(action));
  }

  // pointer properties
  action_sequence.Set("type", "pointer");
  action_sequence.Set("id", "pointer1");
  action_sequence.Set("actions", std::move(actions));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk());

  // check resulting action dictionary
  ASSERT_EQ(3U, action_list.size());
  const base::DictValue& action1 = action_list[0];
  ASSERT_EQ("pointer", base::OptionalFromPtr(action1.FindString("type")));
  ASSERT_EQ("mouse", base::OptionalFromPtr(action1.FindString("pointerType")));
  ASSERT_EQ("pointer1", base::OptionalFromPtr(action1.FindString("id")));
  ASSERT_EQ("pointerMove",
            base::OptionalFromPtr(action1.FindString("subtype")));
  ASSERT_EQ(30, action1.FindDouble("x"));
  ASSERT_EQ(60, action1.FindDouble("y"));

  const base::DictValue& action2 = action_list[1];
  ASSERT_EQ("pointer", base::OptionalFromPtr(action2.FindString("type")));
  ASSERT_EQ("mouse", base::OptionalFromPtr(action2.FindString("pointerType")));
  ASSERT_EQ("pointer1", base::OptionalFromPtr(action2.FindString("id")));
  ASSERT_EQ("pointerDown",
            base::OptionalFromPtr(action2.FindString("subtype")));
  ASSERT_EQ("left", base::OptionalFromPtr(action2.FindString("button")));

  const base::DictValue& action3 = action_list[2];
  ASSERT_EQ("pointer", base::OptionalFromPtr(action3.FindString("type")));
  ASSERT_EQ("mouse", base::OptionalFromPtr(action3.FindString("pointerType")));
  ASSERT_EQ("pointer1", base::OptionalFromPtr(action3.FindString("id")));
  ASSERT_EQ("pointerUp", base::OptionalFromPtr(action3.FindString("subtype")));
  ASSERT_EQ("left", base::OptionalFromPtr(action3.FindString("button")));
}

// Builds a pointer source with one pointerDown action and the supplied common
// properties for validation tests.
static base::DictValue MakePointerDownSequence(base::DictValue pointer_down) {
  base::DictValue action_sequence;
  base::DictValue parameters;
  parameters.Set("pointerType", "mouse");
  action_sequence.Set("parameters", std::move(parameters));
  action_sequence.Set("type", "pointer");
  action_sequence.Set("id", "pointer1");
  base::ListValue actions;
  pointer_down.Set("type", "pointerDown");
  pointer_down.Set("button", 0);
  actions.Append(std::move(pointer_down));
  action_sequence.Set("actions", std::move(actions));
  return action_sequence;
}

TEST(WindowCommandsTest,
     ProcessInputActionSequencePointerParametersInvalidType) {
  std::vector<base::Value> invalid_values;
  invalid_values.emplace_back();
  invalid_values.emplace_back("foo");
  invalid_values.emplace_back(true);
  invalid_values.emplace_back(42);
  invalid_values.emplace_back(base::Value::Type::LIST);
  for (base::Value& invalid_value : invalid_values) {
    SCOPED_TRACE(static_cast<int>(invalid_value.type()));
    Session session("1");
    std::vector<base::DictValue> action_list;
    base::DictValue action_sequence;
    action_sequence.Set("type", "pointer");
    action_sequence.Set("id", "pointer1");
    action_sequence.Set("parameters", std::move(invalid_value));
    action_sequence.Set("actions", base::ListValue());
    Status status =
        ProcessInputActionSequence(&session, action_sequence, &action_list);
    EXPECT_EQ(kInvalidArgument, status.code());
  }
}

TEST(WindowCommandsTest, ProcessInputActionSequencePointerParametersMissing) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action_sequence = MakePointerDownSequence(base::DictValue());
  action_sequence.Remove("parameters");
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1U, action_list.size());
  ASSERT_EQ("mouse",
            base::OptionalFromPtr(action_list[0].FindString("pointerType")));
}

TEST(WindowCommandsTest, ProcessInputActionSequencePointerParametersEmpty) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action_sequence = MakePointerDownSequence(base::DictValue());
  action_sequence.Set("parameters", base::DictValue());
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk()) << status.message();
  ASSERT_EQ(1U, action_list.size());
  EXPECT_EQ("mouse",
            base::OptionalFromPtr(action_list[0].FindString("pointerType")));
}

TEST(WindowCommandsTest,
     ProcessInputActionSequencePointerParametersUnknownKey) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action_sequence = MakePointerDownSequence(base::DictValue());
  base::DictValue parameters;
  parameters.Set("unknown", "ignored");
  action_sequence.Set("parameters", std::move(parameters));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk()) << status.message();
  ASSERT_EQ(1U, action_list.size());
  EXPECT_EQ("mouse",
            base::OptionalFromPtr(action_list[0].FindString("pointerType")));
}

TEST(WindowCommandsTest,
     ProcessInputActionSequencePointerParametersValidPointerTypes) {
  for (const char* pointer_type : {"mouse", "pen", "touch"}) {
    SCOPED_TRACE(pointer_type);
    Session session("1");
    std::vector<base::DictValue> action_list;
    base::DictValue action_sequence =
        MakePointerDownSequence(base::DictValue());
    action_sequence.FindDict("parameters")->Set("pointerType", pointer_type);
    Status status =
        ProcessInputActionSequence(&session, action_sequence, &action_list);
    ASSERT_TRUE(status.IsOk()) << status.message();
    ASSERT_EQ(1U, action_list.size());
    EXPECT_EQ(pointer_type,
              base::OptionalFromPtr(action_list[0].FindString("pointerType")));
  }
}

TEST(WindowCommandsTest,
     ProcessInputActionSequencePointerParametersNullPointerType) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action_sequence = MakePointerDownSequence(base::DictValue());
  action_sequence.FindDict("parameters")->Set("pointerType", base::Value());
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  EXPECT_EQ(kInvalidArgument, status.code());
}

TEST(WindowCommandsTest,
     ProcessInputActionSequencePointerFractionalDimensions) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action;
  action.Set("width", 0.1);
  action.Set("height", 2.5);
  base::DictValue action_sequence = MakePointerDownSequence(std::move(action));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1U, action_list.size());
  ASSERT_EQ(0.1, action_list[0].FindDouble("width"));
  ASSERT_EQ(2.5, action_list[0].FindDouble("height"));
}

TEST(WindowCommandsTest,
     ProcessInputActionSequencePointerDimensionsAboveMaxSafeInteger) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action;
  constexpr double kAboveMaxSafeInteger = 9007199254740992.0;
  action.Set("width", kAboveMaxSafeInteger);
  action.Set("height", kAboveMaxSafeInteger);
  base::DictValue action_sequence = MakePointerDownSequence(std::move(action));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1U, action_list.size());
  ASSERT_EQ(kAboveMaxSafeInteger, action_list[0].FindDouble("width"));
  ASSERT_EQ(kAboveMaxSafeInteger, action_list[0].FindDouble("height"));
}

TEST(WindowCommandsTest,
     ProcessInputActionSequencePointerAltitudeAngleInvalidType) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action;
  action.Set("altitudeAngle", "foo");
  base::DictValue action_sequence = MakePointerDownSequence(std::move(action));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_EQ(kInvalidArgument, status.code());
}

TEST(WindowCommandsTest,
     ProcessInputActionSequencePointerAzimuthAngleInvalidType) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action;
  action.Set("azimuthAngle", base::Value());
  base::DictValue action_sequence = MakePointerDownSequence(std::move(action));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_EQ(kInvalidArgument, status.code());
}

TEST(WindowCommandsTest,
     ProcessInputActionSequencePointerAltitudeAngleNegative) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action;
  action.Set("altitudeAngle", -0.1);
  base::DictValue action_sequence = MakePointerDownSequence(std::move(action));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_EQ(kInvalidArgument, status.code());
}

TEST(WindowCommandsTest,
     ProcessInputActionSequencePointerAltitudeAngleOutOfRange) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action;
  action.Set("altitudeAngle", 2.0);  // Greater than pi/2.
  base::DictValue action_sequence = MakePointerDownSequence(std::move(action));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_EQ(kInvalidArgument, status.code());
}

TEST(WindowCommandsTest,
     ProcessInputActionSequencePointerAzimuthAngleNegative) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action;
  action.Set("azimuthAngle", -0.1);
  base::DictValue action_sequence = MakePointerDownSequence(std::move(action));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_EQ(kInvalidArgument, status.code());
}

TEST(WindowCommandsTest,
     ProcessInputActionSequencePointerAzimuthAngleOutOfRange) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action;
  action.Set("azimuthAngle", 7.0);  // Greater than 2*pi.
  base::DictValue action_sequence = MakePointerDownSequence(std::move(action));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_EQ(kInvalidArgument, status.code());
}

TEST(WindowCommandsTest,
     ProcessInputActionSequencePointerAngleUpperBoundsValid) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action;
  action.Set("altitudeAngle", std::numbers::pi_v<double> / 2);
  action.Set("azimuthAngle", 2 * std::numbers::pi_v<double>);
  base::DictValue action_sequence = MakePointerDownSequence(std::move(action));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1U, action_list.size());
  ASSERT_EQ(std::numbers::pi_v<double> / 2,
            action_list[0].FindDouble("altitudeAngle"));
  ASSERT_EQ(2 * std::numbers::pi_v<double>,
            action_list[0].FindDouble("azimuthAngle"));
}

TEST(WindowCommandsTest,
     ProcessInputActionSequencePointerAngleLowerBoundsValid) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action;
  action.Set("altitudeAngle", 0.0);
  action.Set("azimuthAngle", 0.0);
  base::DictValue action_sequence = MakePointerDownSequence(std::move(action));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1U, action_list.size());
  ASSERT_EQ(0.0, action_list[0].FindDouble("altitudeAngle"));
  ASSERT_EQ(0.0, action_list[0].FindDouble("azimuthAngle"));
}

TEST(WindowCommandsTest, ProcessInputActionSequencePointerAnglesMissing) {
  for (const char* action_type : {"pointerDown", "pointerMove", "pointerUp"}) {
    SCOPED_TRACE(action_type);
    Session session("1");
    std::vector<base::DictValue> action_list;
    base::DictValue action_sequence =
        MakePointerDownSequence(base::DictValue());
    base::DictValue& action =
        (*action_sequence.FindList("actions"))[0].GetDict();
    action.Set("type", action_type);
    if (std::string(action_type) == "pointerMove") {
      action.Set("x", 0);
      action.Set("y", 0);
    }

    Status status =
        ProcessInputActionSequence(&session, action_sequence, &action_list);
    ASSERT_TRUE(status.IsOk()) << status.message();
    ASSERT_EQ(1U, action_list.size());
    EXPECT_FALSE(action_list[0].Find("altitudeAngle"));
    EXPECT_FALSE(action_list[0].Find("azimuthAngle"));
  }
}

TEST(WindowCommandsTest, ProcessInputActionSequencePointerAnglesIndependent) {
  for (const char* action_type : {"pointerDown", "pointerMove", "pointerUp"}) {
    for (bool is_altitude : {true, false}) {
      for (bool is_integer : {true, false}) {
        SCOPED_TRACE(action_type);
        SCOPED_TRACE(is_altitude);
        SCOPED_TRACE(is_integer);
        Session session("1");
        std::vector<base::DictValue> action_list;
        const char* angle = is_altitude ? "altitudeAngle" : "azimuthAngle";
        const char* other_angle =
            is_altitude ? "azimuthAngle" : "altitudeAngle";
        base::DictValue action_sequence =
            MakePointerDownSequence(base::DictValue());
        base::DictValue& action =
            (*action_sequence.FindList("actions"))[0].GetDict();
        action.Set("type", action_type);
        action.Set(angle, is_integer ? base::Value(0) : base::Value(0.5));
        if (std::string(action_type) == "pointerMove") {
          action.Set("x", 0);
          action.Set("y", 0);
        }

        Status status =
            ProcessInputActionSequence(&session, action_sequence, &action_list);
        ASSERT_TRUE(status.IsOk()) << status.message();
        ASSERT_EQ(1U, action_list.size());
        EXPECT_EQ(is_integer ? 0.0 : 0.5, action_list[0].FindDouble(angle));
        EXPECT_FALSE(action_list[0].Find(other_angle));
      }
    }
  }
}

TEST(WindowCommandsTest,
     ProcessInputActionSequencePointerMoveAndUpAnglesInvalidType) {
  for (bool is_move : {true, false}) {
    for (bool is_altitude : {true, false}) {
      Session session("1");
      std::vector<base::DictValue> action_list;
      base::DictValue action;
      action.Set(is_altitude ? "altitudeAngle" : "azimuthAngle",
                 "not a number");
      base::DictValue action_sequence =
          MakePointerDownSequence(std::move(action));
      base::DictValue& pointer_action =
          (*action_sequence.FindList("actions"))[0].GetDict();
      pointer_action.Set("type", is_move ? "pointerMove" : "pointerUp");
      if (is_move) {
        pointer_action.Set("x", 0);
        pointer_action.Set("y", 0);
      }

      SCOPED_TRACE(is_move ? "pointerMove" : "pointerUp");
      SCOPED_TRACE(is_altitude ? "altitudeAngle" : "azimuthAngle");
      Status status =
          ProcessInputActionSequence(&session, action_sequence, &action_list);
      EXPECT_EQ(kInvalidArgument, status.code());
    }
  }
}

TEST(WindowCommandsTest,
     ProcessInputActionSequencePointerMoveAndUpAnglesValid) {
  for (bool is_move : {true, false}) {
    Session session("1");
    std::vector<base::DictValue> action_list;
    base::DictValue action;
    action.Set("altitudeAngle", std::numbers::pi_v<double> / 2);
    action.Set("azimuthAngle", 2 * std::numbers::pi_v<double>);
    base::DictValue action_sequence =
        MakePointerDownSequence(std::move(action));
    base::DictValue& pointer_action =
        (*action_sequence.FindList("actions"))[0].GetDict();
    pointer_action.Set("type", is_move ? "pointerMove" : "pointerUp");
    if (is_move) {
      pointer_action.Set("x", 0);
      pointer_action.Set("y", 0);
    }

    SCOPED_TRACE(is_move ? "pointerMove" : "pointerUp");
    Status status =
        ProcessInputActionSequence(&session, action_sequence, &action_list);
    ASSERT_TRUE(status.IsOk()) << status.message();
    ASSERT_EQ(1U, action_list.size());
    EXPECT_EQ(std::numbers::pi_v<double> / 2,
              action_list[0].FindDouble("altitudeAngle"));
    EXPECT_EQ(2 * std::numbers::pi_v<double>,
              action_list[0].FindDouble("azimuthAngle"));
  }
}

TEST(WindowCommandsTest, ProcessInputActionSequencePointerPauseIgnoresAngles) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action_sequence = MakePointerDownSequence(base::DictValue());
  base::ListValue* actions = action_sequence.FindList("actions");
  ASSERT_TRUE(actions);
  (*actions)[0].GetDict().Set("type", "pause");
  (*actions)[0].GetDict().Set("altitudeAngle", "not a number");
  (*actions)[0].GetDict().Set("azimuthAngle", -1);
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1U, action_list.size());
}

TEST(WindowCommandsTest, ProcessInputActionSequencePointerTouch) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action_sequence;
  base::ListValue actions;
  base::DictValue parameters;
  parameters.Set("pointerType", "touch");
  action_sequence.Set("parameters", std::move(parameters));
  {
    base::DictValue action;
    action.Set("type", "pointerMove");
    action.Set("x", 30);
    action.Set("y", 60);
    actions.Append(std::move(action));
  }
  {
    base::DictValue action;
    action.Set("type", "pointerDown");
    actions.Append(std::move(action));
  }
  {
    base::DictValue action;
    action.Set("type", "pointerUp");
    actions.Append(std::move(action));
  }

  // pointer properties
  action_sequence.Set("type", "pointer");
  action_sequence.Set("id", "pointer1");
  action_sequence.Set("actions", std::move(actions));
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk());

  // check resulting action dictionary
  ASSERT_EQ(3U, action_list.size());
  const base::DictValue& action1 = action_list[0];
  ASSERT_EQ("pointer", base::OptionalFromPtr(action1.FindString("type")));
  ASSERT_EQ("touch", base::OptionalFromPtr(action1.FindString("pointerType")));
  ASSERT_EQ("pointer1", base::OptionalFromPtr(action1.FindString("id")));
  ASSERT_EQ("pointerMove",
            base::OptionalFromPtr(action1.FindString("subtype")));
  ASSERT_EQ(30, action1.FindDouble("x"));
  ASSERT_EQ(60, action1.FindDouble("y"));

  const base::DictValue& action2 = action_list[1];
  ASSERT_EQ("pointer", base::OptionalFromPtr(action2.FindString("type")));
  ASSERT_EQ("touch", base::OptionalFromPtr(action2.FindString("pointerType")));
  ASSERT_EQ("pointer1", base::OptionalFromPtr(action2.FindString("id")));
  ASSERT_EQ("pointerDown",
            base::OptionalFromPtr(action2.FindString("subtype")));

  const base::DictValue& action3 = action_list[2];
  ASSERT_EQ("pointer", base::OptionalFromPtr(action3.FindString("type")));
  ASSERT_EQ("touch", base::OptionalFromPtr(action3.FindString("pointerType")));
  ASSERT_EQ("pointer1", base::OptionalFromPtr(action3.FindString("id")));
  ASSERT_EQ("pointerUp", base::OptionalFromPtr(action3.FindString("subtype")));
}

// Builds a wheel input source sequence with a single scroll action using the
// given origin string.
static base::DictValue MakeWheelScrollSequence(const std::string& origin) {
  base::DictValue action_sequence;
  action_sequence.Set("type", "wheel");
  action_sequence.Set("id", "foo");
  base::ListValue actions;
  base::DictValue action;
  action.Set("type", "scroll");
  action.Set("x", 0);
  action.Set("y", 0);
  action.Set("deltaX", 0);
  action.Set("deltaY", 0);
  action.Set("origin", origin);
  actions.Append(std::move(action));
  action_sequence.Set("actions", std::move(actions));
  return action_sequence;
}

TEST(WindowCommandsTest, ProcessInputActionSequenceWheelScrollOriginPointer) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action_sequence = MakeWheelScrollSequence("pointer");
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_EQ(kInvalidArgument, status.code());
}

TEST(WindowCommandsTest, ProcessInputActionSequenceWheelScrollOriginViewport) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action_sequence = MakeWheelScrollSequence("viewport");
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1U, action_list.size());
  ASSERT_EQ("viewport",
            base::OptionalFromPtr(action_list[0].FindString("origin")));
}

TEST(WindowCommandsTest, ProcessInputActionSequenceWheelIgnoresPointerAngles) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action_sequence = MakeWheelScrollSequence("viewport");
  base::ListValue* actions = action_sequence.FindList("actions");
  ASSERT_TRUE(actions);
  (*actions)[0].GetDict().Set("altitudeAngle", "not a number");
  (*actions)[0].GetDict().Set("azimuthAngle", -1);
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1U, action_list.size());
}

TEST(WindowCommandsTest, ProcessInputActionSequenceWheelIgnoresParameters) {
  Session session("1");
  std::vector<base::DictValue> action_list;
  base::DictValue action_sequence = MakeWheelScrollSequence("viewport");
  action_sequence.Set("parameters", 42);
  Status status =
      ProcessInputActionSequence(&session, action_sequence, &action_list);
  ASSERT_TRUE(status.IsOk()) << status.message();
  ASSERT_EQ(1U, action_list.size());
}

TEST(WindowCommandsTest, ExecuteSetRPHRegistrationMode_NoParams) {
  base::DictValue params;
  Status status = CallWindowCommand(ExecuteSetRPHRegistrationMode, params);
  ASSERT_EQ(kInvalidArgument, status.code());
  ASSERT_NE(status.message().find("missing parameter 'mode'"),
            std::string::npos);
}

TEST(WindowCommandsTest, ExecuteSetRPHRegistrationMode) {
  base::DictValue params;
  params.Set("mode", "autoaccept");
  Status status = CallWindowCommand(ExecuteSetRPHRegistrationMode, params);
  ASSERT_EQ(kOk, status.code());
}

namespace {

class AddCookieWebView : public StubWebView {
 public:
  explicit AddCookieWebView(std::string document_url)
      : StubWebView("1"), document_url_(document_url) {}
  ~AddCookieWebView() override = default;

  Status CallFunction(const std::string& frame,
                      const std::string& function,
                      const base::ListValue& args,
                      std::unique_ptr<base::Value>* result) override {
    if (function.find("document.URL") != std::string::npos) {
      *result = std::make_unique<base::Value>(document_url_);
    }
    return Status(kOk);
  }

 private:
  std::string document_url_;
};

}  // namespace

TEST(WindowCommandsTest, ExecuteAddCookie_Valid) {
  AddCookieWebView webview = AddCookieWebView("http://chromium.org");
  base::DictValue params;
  base::DictValue cookie_params;
  cookie_params.Set("name", "testcookie");
  cookie_params.Set("value", "cookievalue");
  cookie_params.Set("sameSite", "Strict");
  params.Set("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_NameMissing) {
  AddCookieWebView webview = AddCookieWebView("http://chromium.org");
  base::DictValue params;
  base::DictValue cookie_params;
  cookie_params.Set("value", "cookievalue");
  cookie_params.Set("sameSite", "invalid");
  params.Set("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'name'"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_MissingValue) {
  AddCookieWebView webview = AddCookieWebView("http://chromium.org");
  base::DictValue params;
  base::DictValue cookie_params;
  cookie_params.Set("name", "testcookie");
  cookie_params.Set("sameSite", "Strict");
  params.Set("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'value'"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_DomainInvalid) {
  AddCookieWebView webview = AddCookieWebView("file://chromium.org");
  base::DictValue params;
  base::DictValue cookie_params;
  cookie_params.Set("name", "testcookie");
  cookie_params.Set("value", "cookievalue");
  cookie_params.Set("sameSite", "Strict");
  params.Set("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kInvalidCookieDomain, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_SameSiteEmpty) {
  AddCookieWebView webview = AddCookieWebView("https://chromium.org");
  base::DictValue params;
  base::DictValue cookie_params;
  cookie_params.Set("name", "testcookie");
  cookie_params.Set("value", "cookievalue");
  cookie_params.Set("sameSite", "");
  params.Set("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecuteAddCookie_SameSiteNotSet) {
  AddCookieWebView webview = AddCookieWebView("ftp://chromium.org");
  base::DictValue params;
  base::DictValue cookie_params;
  cookie_params.Set("name", "testcookie");
  cookie_params.Set("value", "cookievalue");
  params.Set("cookie", std::move(cookie_params));
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteAddCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
}

namespace {

class GetCookiesWebView : public StubWebView {
 public:
  explicit GetCookiesWebView(std::string document_url)
      : StubWebView("1"), document_url_(document_url) {}
  ~GetCookiesWebView() override = default;

  Status CallFunction(const std::string& frame,
                      const std::string& function,
                      const base::ListValue& args,
                      std::unique_ptr<base::Value>* result) override {
    if (function.find("document.URL") != std::string::npos) {
      *result = std::make_unique<base::Value>(document_url_);
    }
    return Status(kOk);
  }

  Status GetCookies(base::Value* cookies,
                    const std::string& current_page_url) override {
    base::ListValue new_cookies;
    base::DictValue cookie_0;
    cookie_0.Set("name", "a");
    cookie_0.Set("value", "0");
    cookie_0.Set("domain", "example.com");
    cookie_0.Set("path", "/");
    cookie_0.Set("session", true);
    new_cookies.Append(cookie_0.Clone());
    base::DictValue cookie_1;
    cookie_1.Set("name", "b");
    cookie_1.Set("value", "1");
    cookie_1.Set("domain", "example.org");
    cookie_1.Set("path", "/test");
    cookie_1.Set("sameSite", "None");
    cookie_1.Set("expires", 10);
    cookie_1.Set("httpOnly", true);
    cookie_1.Set("session", false);
    cookie_1.Set("secure", true);
    new_cookies.Append(cookie_1.Clone());
    *cookies = base::Value(new_cookies.Clone());
    return Status(kOk);
  }

 private:
  std::string document_url_;
};

}  // namespace

TEST(WindowCommandsTest, ExecuteGetCookies) {
  GetCookiesWebView webview = GetCookiesWebView("https://chromium.org");
  base::DictValue params;
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteGetCookies, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::ListValue expected_cookies;
  base::DictValue cookie_0;
  cookie_0.Set("name", "a");
  cookie_0.Set("value", "0");
  cookie_0.Set("domain", "example.com");
  cookie_0.Set("path", "/");
  cookie_0.Set("sameSite", "Lax");
  cookie_0.Set("httpOnly", false);
  cookie_0.Set("secure", false);
  expected_cookies.Append(cookie_0.Clone());
  base::DictValue cookie_1;
  cookie_1.Set("name", "b");
  cookie_1.Set("value", "1");
  cookie_1.Set("domain", "example.org");
  cookie_1.Set("path", "/test");
  cookie_1.Set("sameSite", "None");
  cookie_1.Set("expiry", 10);
  cookie_1.Set("httpOnly", true);
  cookie_1.Set("secure", true);
  expected_cookies.Append(cookie_1.Clone());
  EXPECT_EQ(result_value->GetList(), expected_cookies);
}

TEST(WindowCommandsTest, ExecuteGetNamedCookie) {
  GetCookiesWebView webview = GetCookiesWebView("https://chromium.org");
  base::DictValue params;
  std::unique_ptr<base::Value> result_value;

  // Get without cookie name.
  Status status =
      CallWindowCommand(ExecuteGetNamedCookie, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  // Get with undefined cookie.
  params.Set("name", "missing");
  status =
      CallWindowCommand(ExecuteGetNamedCookie, &webview, params, &result_value);
  ASSERT_EQ(kNoSuchCookie, status.code()) << status.message();

  // Get cookie a.
  params.Set("name", "a");
  status =
      CallWindowCommand(ExecuteGetNamedCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictValue expected_cookie_0;
  expected_cookie_0.Set("name", "a");
  expected_cookie_0.Set("value", "0");
  expected_cookie_0.Set("domain", "example.com");
  expected_cookie_0.Set("path", "/");
  expected_cookie_0.Set("sameSite", "Lax");
  expected_cookie_0.Set("httpOnly", false);
  expected_cookie_0.Set("secure", false);
  EXPECT_EQ(result_value->GetDict(), expected_cookie_0);

  // Get cookie b.
  params.Set("name", "b");
  status =
      CallWindowCommand(ExecuteGetNamedCookie, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictValue expected_cookie_1;
  expected_cookie_1.Set("name", "b");
  expected_cookie_1.Set("value", "1");
  expected_cookie_1.Set("domain", "example.org");
  expected_cookie_1.Set("path", "/test");
  expected_cookie_1.Set("sameSite", "None");
  expected_cookie_1.Set("expiry", 10);
  expected_cookie_1.Set("httpOnly", true);
  expected_cookie_1.Set("secure", true);
  EXPECT_EQ(result_value->GetDict(), expected_cookie_1);
}

namespace {

class StorePrintParamsWebView : public StubWebView {
 public:
  StorePrintParamsWebView() : StubWebView("1") {}
  ~StorePrintParamsWebView() override = default;

  Status PrintToPDF(const base::DictValue& params, std::string* pdf) override {
    params_ = base::Value(params.Clone());
    return Status(kOk);
  }

  const base::Value& GetParams() const { return params_; }

 private:
  base::Value params_;
};

base::DictValue GetDefaultPrintParams() {
  base::DictValue dict;
  dict.Set("landscape", false);
  dict.Set("scale", 1.0);
  dict.Set("marginBottom", ConvertCentimeterToInch(1.0));
  dict.Set("marginLeft", ConvertCentimeterToInch(1.0));
  dict.Set("marginRight", ConvertCentimeterToInch(1.0));
  dict.Set("marginTop", ConvertCentimeterToInch(1.0));
  dict.Set("paperHeight", ConvertCentimeterToInch(27.94));
  dict.Set("paperWidth", ConvertCentimeterToInch(21.59));
  dict.Set("pageRanges", "");
  dict.Set("preferCSSPageSize", false);
  dict.Set("printBackground", false);
  dict.Set("transferMode", "ReturnAsBase64");
  return dict;
}
}  // namespace

TEST(WindowCommandsTest, ExecutePrintDefaultParams) {
  StorePrintParamsWebView webview;
  base::DictValue params;
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictValue print_params = GetDefaultPrintParams();
  ASSERT_EQ(print_params, webview.GetParams());
}

TEST(WindowCommandsTest, ExecutePrintSpecifyOrientation) {
  StorePrintParamsWebView webview;
  base::DictValue params;
  std::unique_ptr<base::Value> result_value;

  params.Set("orientation", "portrait");
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictValue print_params = GetDefaultPrintParams();
  ASSERT_EQ(print_params, webview.GetParams());

  params.Set("orientation", "landscape");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("landscape", true);
  ASSERT_EQ(print_params, webview.GetParams());

  params.Set("orientation", "Invalid");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  params.Set("orientation", true);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecutePrintSpecifyScale) {
  StorePrintParamsWebView webview;
  base::DictValue params;
  std::unique_ptr<base::Value> result_value;

  params.Set("scale", 1.0);
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictValue print_params = GetDefaultPrintParams();
  ASSERT_EQ(print_params, webview.GetParams());

  params.Set("scale", 2.0);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("scale", 2.0);
  ASSERT_EQ(print_params, webview.GetParams());

  params.Set("scale", 0.05);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  params.Set("scale", 2.1);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  params.Set("scale", "1.3");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecutePrintSpecifyBackground) {
  StorePrintParamsWebView webview;
  base::DictValue params;
  std::unique_ptr<base::Value> result_value;

  params.Set("background", false);
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictValue print_params = GetDefaultPrintParams();
  ASSERT_EQ(print_params, webview.GetParams());

  params.Set("background", true);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("printBackground", true);
  ASSERT_EQ(print_params, webview.GetParams());

  params.Set("background", "true");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  params.Set("background", 2);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecutePrintSpecifyShrinkToFit) {
  StorePrintParamsWebView webview;
  base::DictValue params;
  std::unique_ptr<base::Value> result_value;

  params.Set("shrinkToFit", true);
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictValue print_params = GetDefaultPrintParams();
  ASSERT_EQ(print_params, webview.GetParams());

  params.Set("shrinkToFit", false);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("preferCSSPageSize", true);
  ASSERT_EQ(print_params, webview.GetParams());

  params.Set("shrinkToFit", "False");
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  params.Set("shrinkToFit", 2);
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecutePrintSpecifyPageRanges) {
  StorePrintParamsWebView webview;
  base::DictValue params;
  std::unique_ptr<base::Value> result_value;

  base::ListValue lv;
  params.Set("pageRanges", std::move(lv));
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictValue print_params = GetDefaultPrintParams();
  ASSERT_EQ(print_params, webview.GetParams());

  lv = base::ListValue();
  lv.Append(2);
  lv.Append(1);
  lv.Append(3);
  lv.Append("4-4");
  lv.Append("4-");
  lv.Append("-5");
  params.Set("pageRanges", std::move(lv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("pageRanges", "2,1,3,4-4,4-,-5");
  ASSERT_EQ(print_params, webview.GetParams());

  lv = base::ListValue();
  lv.Append(-1);
  params.Set("pageRanges", std::move(lv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  lv = base::ListValue();
  lv.Append(3.0);
  params.Set("pageRanges", std::move(lv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  lv = base::ListValue();
  lv.Append(true);
  params.Set("pageRanges", std::move(lv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  // ExecutePrint delegates invalid string checks to CDP
  lv = base::ListValue();
  lv.Append("-");
  lv.Append("");
  lv.Append("  ");
  lv.Append(" 1-3 ");
  lv.Append("Invalid");
  params.Set("pageRanges", std::move(lv));
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("pageRanges", "-,,  , 1-3 ,Invalid");
  ASSERT_EQ(print_params, webview.GetParams());
}

TEST(WindowCommandsTest, ExecutePrintSpecifyPage) {
  StorePrintParamsWebView webview;
  base::DictValue params;
  std::unique_ptr<base::Value> result_value;

  params.Set("page", base::DictValue());
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictValue print_params = GetDefaultPrintParams();
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::DictValue dv;
    dv.Set("width", 21.59);
    params.Set("page", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("paperWidth", ConvertCentimeterToInch(21.59));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::DictValue dv;
    dv.Set("width", 33);
    params.Set("page", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("paperWidth", ConvertCentimeterToInch(33));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::DictValue dv;
    dv.Set("width", "10");
    params.Set("page", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::DictValue dv;
    dv.Set("width", -3.0);
    params.Set("page", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::DictValue dv;
    dv.Set("height", 20);
    params.Set("page", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("paperHeight", ConvertCentimeterToInch(20));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::DictValue dv;
    dv.Set("height", 27.94);
    params.Set("page", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("paperHeight", ConvertCentimeterToInch(27.94));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::DictValue dv;
    dv.Set("height", "10");
    params.Set("page", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::DictValue dv;
    dv.Set("height", -3.0);
    params.Set("page", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

TEST(WindowCommandsTest, ExecutePrintSpecifyMargin) {
  StorePrintParamsWebView webview;
  base::DictValue params;
  std::unique_ptr<base::Value> result_value;

  params.Set("margin", base::DictValue());
  Status status =
      CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictValue print_params = GetDefaultPrintParams();
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::DictValue dv;
    dv.Set("top", 1.0);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("marginTop", ConvertCentimeterToInch(1.0));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::DictValue dv;
    dv.Set("top", 10.2);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("marginTop", ConvertCentimeterToInch(10.2));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::DictValue dv;
    dv.Set("top", "10.2");
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::DictValue dv;
    dv.Set("top", -0.1);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::DictValue dv;
    dv.Set("bottom", 1.0);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("marginBottom", ConvertCentimeterToInch(1.0));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::DictValue dv;
    dv.Set("bottom", 5.3);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("marginBottom", ConvertCentimeterToInch(5.3));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::DictValue dv;
    dv.Set("bottom", "10.2");
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::DictValue dv;
    dv.Set("bottom", -0.1);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::DictValue dv;
    dv.Set("left", 1.0);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("marginLeft", ConvertCentimeterToInch(1.0));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::DictValue dv;
    dv.Set("left", 9.1);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("marginLeft", ConvertCentimeterToInch(9.1));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::DictValue dv;
    dv.Set("left", "10.2");
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::DictValue dv;
    dv.Set("left", -0.1);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::DictValue dv;
    dv.Set("right", 1.0);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("marginRight", ConvertCentimeterToInch(1.0));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::DictValue dv;
    dv.Set("right", 8.1);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  print_params = GetDefaultPrintParams();
  print_params.Set("marginRight", ConvertCentimeterToInch(8.1));
  ASSERT_EQ(print_params, webview.GetParams());

  {
    base::DictValue dv;
    dv.Set("right", "10.2");
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();

  {
    base::DictValue dv;
    dv.Set("right", -0.1);
    params.Set("margin", std::move(dv));
  }
  status = CallWindowCommand(ExecutePrint, &webview, params, &result_value);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
}

namespace {
constexpr double wd = 345.6;
constexpr double hd = 5432.1;
constexpr int wi = 346;
constexpr int hi = 5433;
constexpr bool mobile = false;
constexpr double device_scale_factor = 0.3;

class StoreScreenshotParamsWebView : public StubWebView {
 public:
  explicit StoreScreenshotParamsWebView(
      DevToolsClient* dtc = nullptr,
      std::optional<MobileDevice> md = std::nullopt)
      : StubWebView("1"),
        meom_(new MobileEmulationOverrideManager(dtc, md, 0)) {}
  ~StoreScreenshotParamsWebView() override = default;

  Status SendCommandAndGetResult(const std::string& cmd,
                                 const base::DictValue& params,
                                 std::unique_ptr<base::Value>* value) override {
    if (cmd == "Page.getLayoutMetrics") {
      base::DictValue res;
      base::DictValue d;
      d.Set("width", wd);
      d.Set("height", hd);
      res.Set("contentSize", std::move(d));
      *value = std::make_unique<base::Value>(std::move(res));
    } else if (cmd == "Emulation.setDeviceMetricsOverride") {
      base::DictValue expect;
      expect.Set("width", wi);
      expect.Set("height", hi);
      if (meom_->HasOverrideMetrics()) {
        expect.Set("deviceScaleFactor", device_scale_factor);
        expect.Set("mobile", mobile);
      } else {
        expect.Set("deviceScaleFactor", 1);
        expect.Set("mobile", false);
      }
      if (expect != params)
        return Status(kInvalidArgument);
    }

    return Status(kOk);
  }

  Status CaptureScreenshot(std::string* screenshot,
                           const base::DictValue& params) override {
    params_ = base::Value(params.Clone());
    return Status(kOk);
  }

  const base::Value& GetParams() const { return params_; }

  MobileEmulationOverrideManager* GetMobileEmulationOverrideManager()
      const override {
    return meom_.get();
  }

 private:
  base::Value params_;
  std::unique_ptr<MobileEmulationOverrideManager> meom_;
};

base::DictValue GetExpectedCaptureParams() {
  base::DictValue clip;
  return clip;
}
}  // namespace

TEST(WindowCommandsTest, ExecuteScreenCapture) {
  StoreScreenshotParamsWebView webview;
  base::DictValue params;
  std::unique_ptr<base::Value> result_value;
  Status status =
      CallWindowCommand(ExecuteScreenshot, &webview, params, &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::DictValue screenshot_params;
  ASSERT_EQ(screenshot_params, webview.GetParams());
}

TEST(WindowCommandsTest, ExecuteFullPageScreenCapture) {
  StoreScreenshotParamsWebView webview;
  base::DictValue params;
  std::unique_ptr<base::Value> result_value;
  Status status = CallWindowCommand(ExecuteFullPageScreenshot, &webview, params,
                                    &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(GetExpectedCaptureParams(), webview.GetParams());
}

TEST(WindowCommandsTest, ExecuteMobileFullPageScreenCapture) {
  StubDevToolsClient sdtc;
  MobileDevice mobile_device;
  mobile_device.device_metrics =
      DeviceMetrics(0, 0, device_scale_factor, false, mobile);
  StoreScreenshotParamsWebView webview(&sdtc, std::move(mobile_device));
  ASSERT_EQ(webview.GetMobileEmulationOverrideManager()->HasOverrideMetrics(),
            true);
  base::DictValue params;
  std::unique_ptr<base::Value> result_value;
  Status status = CallWindowCommand(ExecuteFullPageScreenshot, &webview, params,
                                    &result_value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(GetExpectedCaptureParams(), webview.GetParams());
}

TEST(WindowCommandsTest, ExecuteScript_NoScript) {
  base::DictValue params;
  params.Set("args", base::ListValue());
  Status status = CallWindowCommand(ExecuteExecuteScript, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'script' must be a string"),
            std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteScript_ScriptNotAString) {
  base::DictValue params;
  params.Set("script", base::DictValue());
  params.Set("args", base::ListValue());
  Status status = CallWindowCommand(ExecuteExecuteScript, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'script' must be a string"),
            std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteScript_NoArgs) {
  base::DictValue params;
  params.Set("script", "irrelevant");
  Status status = CallWindowCommand(ExecuteExecuteScript, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'args' must be a list"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteScript_ArgsNotAList) {
  base::DictValue params;
  params.Set("script", "irrelevant");
  params.Set("args", "not-a-list");
  Status status = CallWindowCommand(ExecuteExecuteScript, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'args' must be a list"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteAsyncScript_NoScript) {
  base::DictValue params;
  params.Set("args", base::ListValue());
  Status status = CallWindowCommand(ExecuteExecuteAsyncScript, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'script' must be a string"),
            std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteAsyncScript_ScriptNotAString) {
  base::DictValue params;
  params.Set("script", base::DictValue());
  params.Set("args", base::ListValue());
  Status status = CallWindowCommand(ExecuteExecuteAsyncScript, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'script' must be a string"),
            std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteAsyncScript_NoArgs) {
  base::DictValue params;
  params.Set("script", "irrelevant");
  Status status = CallWindowCommand(ExecuteExecuteAsyncScript, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'args' must be a list"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecuteAsyncScript_ArgsNotAList) {
  base::DictValue params;
  params.Set("script", "irrelevant");
  params.Set("args", "not-a-list");
  Status status = CallWindowCommand(ExecuteExecuteAsyncScript, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'args' must be a list"), std::string::npos)
      << status.message();
}

namespace {

class AsyncTimeoutWebView : public StubWebView {
 public:
  AsyncTimeoutWebView() : StubWebView("1") {}
  ~AsyncTimeoutWebView() override = default;

  Status CallUserAsyncFunction(const std::string& frame,
                               const std::string& function,
                               const base::ListValue& args,
                               const base::TimeDelta& timeout,
                               std::unique_ptr<base::Value>* result) override {
    // Simulate a driver-level timeout for async script execution.
    return Status(kTimeout);
  }
};

}  // namespace

TEST(WindowCommandsTest, ExecuteAsyncScript_TimeoutMapsToScriptTimeout) {
  AsyncTimeoutWebView webview;
  base::DictValue params;
  params.Set("script", "irrelevant");
  params.Set("args", base::ListValue());
  std::unique_ptr<base::Value> result;
  Status status =
      CallWindowCommand(ExecuteExecuteAsyncScript, &webview, params, &result);
  ASSERT_EQ(kScriptTimeout, status.code()) << status.message();
}

TEST(WindowCommandsTest, SendKeysToActiveElement_NoValue) {
  base::DictValue params;
  Status status = CallWindowCommand(ExecuteSendKeysToActiveElement, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'value' must be a list"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, SendKeysToActiveElement_ValueNotAList) {
  base::DictValue params;
  params.Set("value", base::DictValue());
  Status status = CallWindowCommand(ExecuteSendKeysToActiveElement, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'value' must be a list"), std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecutePerformActions_NoActions) {
  base::DictValue params;
  Status status = CallWindowCommand(ExecutePerformActions, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'actions' must be a list"),
            std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecutePerformActions_ActionsNotAList) {
  base::DictValue params;
  params.Set("actions", 7);
  Status status = CallWindowCommand(ExecutePerformActions, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'actions' must be a list"),
            std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecutePerformActions_NoActionsInSequence) {
  base::DictValue sequence;
  sequence.Set("id", "irrelevant");
  sequence.Set("type", "none");
  base::ListValue actions;
  actions.Append(sequence.Clone());
  base::DictValue params;
  params.Set("actions", actions.Clone());
  Status status = CallWindowCommand(ExecutePerformActions, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'actions' in the sequence must be a list"),
            std::string::npos)
      << status.message();
}

TEST(WindowCommandsTest, ExecutePerformActions_ActionsInSequenceNotAList) {
  base::DictValue sequence;
  sequence.Set("id", "irrelevant");
  sequence.Set("type", "none");
  sequence.Set("actions", base::DictValue());
  base::ListValue actions;
  actions.Append(sequence.Clone());
  base::DictValue params;
  params.Set("actions", actions.Clone());
  Status status = CallWindowCommand(ExecutePerformActions, params);
  ASSERT_EQ(kInvalidArgument, status.code()) << status.message();
  ASSERT_NE(status.message().find("'actions' in the sequence must be a list"),
            std::string::npos)
      << status.message();
}
