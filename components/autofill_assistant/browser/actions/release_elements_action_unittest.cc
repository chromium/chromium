// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/release_elements_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::Pointee;
using ::testing::Property;

const char kClientId[] = "1";
const char kOtherClientId[] = "2";

class ReleaseElementsActionTest : public testing::Test {
 public:
  ReleaseElementsActionTest() {}

  void SetUp() override {
    ClientIdProto client_id;
    client_id.set_identifier(kClientId);
    *proto_.add_client_ids() = client_id;
  }

  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_release_elements() = proto_;
    ReleaseElementsAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

 protected:
  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ReleaseElementsProto proto_;
};

TEST_F(ReleaseElementsActionTest, ReleasesSingleElement) {
  ElementFinderResult element;
  element.SetObjectId("id");
  mock_action_delegate_.GetElementStore()->AddElement(kClientId,
                                                      element.dom_object());

  EXPECT_TRUE(mock_action_delegate_.GetElementStore()->HasElement(kClientId));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  Run();

  EXPECT_FALSE(mock_action_delegate_.GetElementStore()->HasElement(kClientId));
}

TEST_F(ReleaseElementsActionTest, ReleasesMultipleElements) {
  ClientIdProto client_id_2;
  client_id_2.set_identifier(kOtherClientId);
  *proto_.add_client_ids() = client_id_2;

  ElementFinderResult element;
  element.SetObjectId("id");
  mock_action_delegate_.GetElementStore()->AddElement(kClientId,
                                                      element.dom_object());
  mock_action_delegate_.GetElementStore()->AddElement(kOtherClientId,
                                                      element.dom_object());

  EXPECT_TRUE(mock_action_delegate_.GetElementStore()->HasElement(kClientId));
  EXPECT_TRUE(
      mock_action_delegate_.GetElementStore()->HasElement(kOtherClientId));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  Run();

  EXPECT_FALSE(mock_action_delegate_.GetElementStore()->HasElement(kClientId));
  EXPECT_FALSE(
      mock_action_delegate_.GetElementStore()->HasElement(kOtherClientId));
}

}  // namespace
}  // namespace autofill_assistant
