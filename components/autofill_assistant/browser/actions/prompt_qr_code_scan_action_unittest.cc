// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/prompt_qr_code_scan_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/value_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

const char kDefaultQrCodeScanResult[] = "qr_code_scan_result";
const char kDefaultOutputClientMemoryKey[] = "client_memory_key";

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;

class PromptQrCodeScanActionTest : public testing::Test {
 public:
  PromptQrCodeScanActionTest() = default;

  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetUserModel)
        .WillByDefault(Return(&user_model_));
  }

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_prompt_qr_code_scan() = proto_;
    PromptQrCodeScanAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  void setCameraScanUiStrings() {
    proto_.mutable_camera_scan_ui_strings()->set_title_text("Title text");
    proto_.mutable_camera_scan_ui_strings()->set_permission_text(
        "Permission text");
    proto_.mutable_camera_scan_ui_strings()->set_permission_button_text(
        "Permission button text");
    proto_.mutable_camera_scan_ui_strings()->set_open_settings_text(
        "Open settings text");
    proto_.mutable_camera_scan_ui_strings()->set_open_settings_button_text(
        "Open settings button text");
    proto_.mutable_camera_scan_ui_strings()
        ->set_camera_preview_instruction_text(
            "Camera preview instruction text");
    proto_.mutable_camera_scan_ui_strings()->set_camera_preview_security_text(
        "Camera preview security text");
  }

  void setImagePickerUiStrings() {
    proto_.mutable_image_picker_ui_strings()->set_title_text("Title text");
    proto_.mutable_image_picker_ui_strings()->set_permission_text(
        "Permission text");
    proto_.mutable_image_picker_ui_strings()->set_permission_button_text(
        "Permission button text");
    proto_.mutable_image_picker_ui_strings()->set_open_settings_text(
        "Open settings text");
    proto_.mutable_image_picker_ui_strings()->set_open_settings_button_text(
        "Open settings button text");
  }

  void testQrCodeSuccessfullyPromptsAndGetsScanResult() {
    InSequence seq;
    EXPECT_CALL(mock_action_delegate_, Prompt).Times(1);
    EXPECT_CALL(mock_action_delegate_, ShowQrCodeScanUi)
        .WillOnce(RunOnceCallback<1>(
            ClientStatus(ACTION_APPLIED),
            SimpleValue(std::string(kDefaultQrCodeScanResult),
                        /* is_client_side_only= */ true)));

    EXPECT_CALL(mock_action_delegate_, ClearQrCodeScanUi).Times(1);
    EXPECT_CALL(mock_action_delegate_, CleanUpAfterPrompt).Times(1);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

    Run();

    EXPECT_EQ(*user_model_.GetValue(kDefaultOutputClientMemoryKey),
              SimpleValue(std::string(kDefaultQrCodeScanResult),
                          /* is_client_side_only= */ true));
  }

  UserModel user_model_;
  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  PromptQrCodeScanProto proto_;
};

TEST_F(PromptQrCodeScanActionTest,
       SuccessfullyPromptsAndGetQrCodeCameraScanResult) {
  proto_.set_use_gallery(false);
  proto_.set_output_client_memory_key(kDefaultOutputClientMemoryKey);
  setCameraScanUiStrings();

  testQrCodeSuccessfullyPromptsAndGetsScanResult();
}

TEST_F(PromptQrCodeScanActionTest,
       SuccessfullyPromptsAndGetQrCodeImagePickerResult) {
  proto_.set_use_gallery(true);
  proto_.set_output_client_memory_key(kDefaultOutputClientMemoryKey);
  setImagePickerUiStrings();

  testQrCodeSuccessfullyPromptsAndGetsScanResult();
}

TEST_F(PromptQrCodeScanActionTest, FailsWhenOutputClientMemoryKeyIsNotSet) {
  proto_.set_use_gallery(false);
  setCameraScanUiStrings();

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));

  Run();
}

TEST_F(PromptQrCodeScanActionTest, FailsWhenCameraScanUiStringsAreNotSet) {
  proto_.set_use_gallery(false);
  proto_.set_output_client_memory_key(kDefaultOutputClientMemoryKey);
  // Should be ignored because use_gallery=false.
  setImagePickerUiStrings();

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));

  Run();
}

TEST_F(PromptQrCodeScanActionTest, FailsWhenImagePickerUiStringsAreNotSet) {
  proto_.set_use_gallery(true);
  proto_.set_output_client_memory_key(kDefaultOutputClientMemoryKey);
  // Should be ignored because use_gallery=true.
  setCameraScanUiStrings();

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));

  Run();
}

}  // namespace
}  // namespace autofill_assistant