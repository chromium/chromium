// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/set_touchable_area_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::SaveArgPointee;

class SetTouchableAreaActionTest : public testing::Test {
 public:
  SetTouchableAreaActionTest() {}
  void SetUp() override {}

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_set_touchable_area() = proto_;
    SetTouchableAreaAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  SetTouchableAreaProto proto_;
};

TEST_F(SetTouchableAreaActionTest, SetsTouchableElementArea) {
  Selector touchable_element({"#id"});
  *proto_.mutable_element_area()->add_touchable()->add_elements() =
      touchable_element.proto;

  EXPECT_CALL(mock_action_delegate_, SetTouchableElementArea(_));

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run(_)).WillOnce(testing::SaveArgPointee<0>(&capture));
  Run();

  EXPECT_EQ(capture.status(), ACTION_APPLIED);
}

}  // namespace
}  // namespace autofill_assistant
