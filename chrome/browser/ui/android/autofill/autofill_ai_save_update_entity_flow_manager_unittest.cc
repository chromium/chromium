// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_ai_save_update_entity_flow_manager.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/android/save_update_address_profile_prompt_mode.h"
#include "chrome/browser/ui/autofill/mock_autofill_message_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::_;
using ::testing::SaveArgByMove;

// TODO: crbug.com/460410690 - Cover different entity types.
class AutofillAiSaveUpdateEntityFlowManagerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutofillAiSaveUpdateEntityFlowManagerTest() {
    flow_manager_ = std::make_unique<AutofillAiSaveUpdateEntityFlowManager>(
        web_contents(), &autofill_message_controller_);
  }
  ~AutofillAiSaveUpdateEntityFlowManagerTest() override = default;

  MockAutofillMessageController& message_controller() {
    return autofill_message_controller_;
  }

  AutofillAiSaveUpdateEntityFlowManager& flow_manager() {
    return *flow_manager_;
  }

  base::MockCallback<AutofillClient::EntityImportPromptResultCallback>&
  prompt_closed_callback() {
    return prompt_closed_callback_;
  }

 private:
  MockAutofillMessageController autofill_message_controller_;
  base::MockCallback<AutofillClient::EntityImportPromptResultCallback>
      prompt_closed_callback_;
  std::unique_ptr<AutofillAiSaveUpdateEntityFlowManager> flow_manager_;
};

TEST_F(AutofillAiSaveUpdateEntityFlowManagerTest, ShowsMessage_MessageIngored) {
  test::PassportEntityOptions new_entity_options;
  new_entity_options.name = u"Jon doe";
  EntityInstance new_entity =
      test::GetPassportEntityInstance(new_entity_options);
  std::unique_ptr<AutofillMessageModel> message_model;

  // Show the message and save the message model.
  EXPECT_CALL(message_controller(), Show(_))
      .WillOnce(SaveArgByMove<0>(&message_model));
  flow_manager().OfferSave(new_entity, prompt_closed_callback().Get());

  // Simulate the user ignoring the message which dismissed it.
  EXPECT_CALL(
      prompt_closed_callback(),
      Run(AutofillClient::AutofillAiBubbleClosedReason::kNotInteracted));
  message_model->OnDismissed(messages::DismissReason::TIMER);
}

TEST_F(AutofillAiSaveUpdateEntityFlowManagerTest, ShowsMessage_MessageClosed) {
  test::PassportEntityOptions new_entity_options;
  new_entity_options.name = u"Jon doe";
  EntityInstance new_entity =
      test::GetPassportEntityInstance(new_entity_options);
  std::unique_ptr<AutofillMessageModel> message_model;

  // Show the message and save the message model.
  EXPECT_CALL(message_controller(), Show(_))
      .WillOnce(SaveArgByMove<0>(&message_model));
  flow_manager().OfferSave(new_entity, prompt_closed_callback().Get());

  // Simulate the swipe on the message that closes it.
  EXPECT_CALL(prompt_closed_callback(),
              Run(AutofillClient::AutofillAiBubbleClosedReason::kClosed));
  message_model->OnDismissed(messages::DismissReason::GESTURE);
}

}  // namespace autofill
