// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/projector_app/annotator_message_handler.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kWebUIListenerCall[] = "cr.webUIListenerCallback";

}  // namespace

class AnnotatorMessageHandlerTest : public testing::Test {
 public:
  AnnotatorMessageHandlerTest() = default;
  AnnotatorMessageHandlerTest(const AnnotatorMessageHandlerTest&) = delete;
  AnnotatorMessageHandler& operator=(const AnnotatorMessageHandlerTest&) =
      delete;
  ~AnnotatorMessageHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    message_handler_ = std::make_unique<AnnotatorMessageHandler>();
    message_handler_->set_web_ui_for_test(&web_ui());
    message_handler_->RegisterMessages();
  }

  void TearDown() override { message_handler_.reset(); }

  void ExpectCallToWebUI(const std::string& type,
                         const std::string& func_name,
                         size_t count) {
    EXPECT_EQ(web_ui().call_data().size(), count);
    const content::TestWebUI::CallData& call_data = *(web_ui().call_data()[0]);
    EXPECT_EQ(call_data.function_name(), type);
    EXPECT_EQ(call_data.arg1()->GetString(), func_name);
  }

  void SendUndoRedoAvailableChanged(bool undo_available, bool redo_available) {
    base::ListValue list_args;
    list_args.Append(base::Value(undo_available));
    list_args.Append(base::Value(redo_available));
    web_ui().HandleReceivedMessage("OnUndoRedoAvailabilityChanged", &list_args);
  }

  content::TestWebUI& web_ui() { return web_ui_; }
  AnnotatorMessageHandler* handler() { return message_handler_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<AnnotatorMessageHandler> message_handler_;
  content::TestWebUI web_ui_;
};

TEST_F(AnnotatorMessageHandlerTest, SetTool) {
  Tool expected_tool;
  expected_tool.color = "black";
  expected_tool.size = 5;
  expected_tool.type = AnnotatorToolType::kPen;
  handler()->SetTool(expected_tool);

  // Let's check that the call has been made.
  ExpectCallToWebUI(kWebUIListenerCall, "setTool", /* call_count = */ 1u);
  const content::TestWebUI::CallData& call_data = *(web_ui().call_data()[0]);

  Tool requested_tool = Tool::ToTool(*call_data.arg2());
  EXPECT_EQ(requested_tool, expected_tool);

  // Now let's check that when the tool has been set, we notify the callback.
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  handler()->SetOnToolSetCallback(base::BindLambdaForTesting(
      [&quit_closure, &expected_tool](const Tool& result_tool) {
        EXPECT_EQ(result_tool, expected_tool);
        quit_closure.Run();
      }));

  base::ListValue list_args;
  list_args.Append(Tool::ToValue(expected_tool));
  web_ui().HandleReceivedMessage("onToolSet", &list_args);
  run_loop.Run();
}

TEST_F(AnnotatorMessageHandlerTest, Undo) {
  handler()->Undo();
  ExpectCallToWebUI(kWebUIListenerCall, "undo", /* call_count = */ 1u);
}

TEST_F(AnnotatorMessageHandlerTest, Redo) {
  handler()->Redo();
  ExpectCallToWebUI(kWebUIListenerCall, "redo", /* call_count = */ 1u);
}

TEST_F(AnnotatorMessageHandlerTest, Clear) {
  handler()->Clear();
  ExpectCallToWebUI(kWebUIListenerCall, "clear", /* call_count = */ 1u);
}

TEST_F(AnnotatorMessageHandlerTest, UndoRedoAvailabilityChanged) {
  bool expected_undo_available = false;
  bool expected_redo_available = false;
  base::RepeatingClosure quit_closure;

  handler()->SetUndoRedoAvailabilityCallback(
      base::BindLambdaForTesting([&](bool undo_available, bool redo_available) {
        EXPECT_EQ(expected_undo_available, undo_available);
        EXPECT_EQ(expected_redo_available, redo_available);
        quit_closure.Run();
      }));

  base::RunLoop run_loop1;
  quit_closure = run_loop1.QuitClosure();
  SendUndoRedoAvailableChanged(false, false);
  run_loop1.Run();

  base::RunLoop run_loop2;
  quit_closure = run_loop2.QuitClosure();
  expected_undo_available = true;
  expected_redo_available = true;
  SendUndoRedoAvailableChanged(true, true);
  run_loop2.Run();

  base::RunLoop run_loop3;
  quit_closure = run_loop3.QuitClosure();
  expected_undo_available = false;
  expected_redo_available = true;
  SendUndoRedoAvailableChanged(false, true);
  run_loop3.Run();
}

}  // namespace chromeos
