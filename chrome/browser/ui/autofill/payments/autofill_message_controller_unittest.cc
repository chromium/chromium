// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_message_controller.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/autofill/payments/autofill_message_controller_test_api.h"
#include "chrome/browser/ui/autofill/payments/autofill_message_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
class AutofillMessageControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  AutofillMessageControllerTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    messages::MessageDispatcherBridge::SetInstanceForTesting(
        &message_dispatcher_bridge_);
  }

  raw_ptr<AutofillMessageModel> CreateAndShowNewMessage() {
    std::unique_ptr<AutofillMessageModel> message =
        AutofillMessageModel::CreateForSaveCardFailure();
    raw_ptr<AutofillMessageModel> message_ptr = message.get();

    controller().Show(std::move(message));

    return message_ptr;
  }

  // Expect a `EnqueueMessage` call to the message dispatcher bridge `times`
  // number of times.
  void ExpectEnqueueMessageCall(int times = 1) {
    EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage).Times(times);
  }

  // Expect a `DismissMessage` call to the message dispatcher bridge with
  // `reason` as the dismiss reason and `times` number of times.
  void ExpectDismissMessageCallWithReason(messages::DismissReason reason,
                                          int times = 1) {
    EXPECT_CALL(message_dispatcher_bridge_, DismissMessage(testing::_, reason))
        .Times(times);
  }

  // Find a message model owned by the controller.
  raw_ptr<AutofillMessageModel> FindMessageModel(
      AutofillMessageModel* message_model_ptr) {
    auto message_models = test_api(controller()).GetMessageModels();
    auto it = message_models.find(message_model_ptr);
    if (it == message_models.end()) {
      return nullptr;
    }
    return (*it).get();
  }

  AutofillMessageController& controller() {
    if (!controller_) {
      controller_ = new AutofillMessageController(web_contents());
    }
    return *controller_;
  }

  std::set<raw_ptr<AutofillMessageModel>> message_models() {
    return test_api(controller()).GetMessageModels();
  }

 private:
  raw_ptr<AutofillMessageController> controller_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
};

TEST_F(AutofillMessageControllerTest, Show) {
  ExpectEnqueueMessageCall();

  raw_ptr<AutofillMessageModel> message = CreateAndShowNewMessage();

  EXPECT_EQ(FindMessageModel(message), message);
}

TEST_F(AutofillMessageControllerTest, ShowTwiceWithoutDismiss) {
  ExpectEnqueueMessageCall(/*times=*/2);

  CreateAndShowNewMessage();
  CreateAndShowNewMessage();

  EXPECT_THAT(message_models(), testing::SizeIs(2));
}

TEST_F(AutofillMessageControllerTest, OnDismissed) {
  raw_ptr<AutofillMessageModel> first_message = CreateAndShowNewMessage();
  raw_ptr<AutofillMessageModel> second_message = CreateAndShowNewMessage();

  controller().OnDismissed(first_message,
                           messages::DismissReason::PRIMARY_ACTION);

  EXPECT_THAT(message_models(), testing::SizeIs(1));
  EXPECT_EQ(FindMessageModel(second_message), second_message);
}

TEST_F(AutofillMessageControllerTest, Dismiss) {
  CreateAndShowNewMessage();
  CreateAndShowNewMessage();

  ExpectDismissMessageCallWithReason(messages::DismissReason::UNKNOWN,
                                     /*times=*/2);

  test_api(controller()).Dismiss();
}

TEST_F(AutofillMessageControllerTest, DismissWithoutMessages) {
  ExpectDismissMessageCallWithReason(messages::DismissReason::UNKNOWN,
                                     /*times=*/0);

  test_api(controller()).Dismiss();
}

TEST_F(AutofillMessageControllerTest, Metrics_Show) {
  base::HistogramTester histogram_tester;
  raw_ptr<AutofillMessageModel> message = CreateAndShowNewMessage();

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.Message.", message->GetTypeAsString(), ".Shown"}),
      true, 1);
}

TEST_F(AutofillMessageControllerTest, Metrics_OnActionClicked) {
  base::HistogramTester histogram_tester;
  raw_ptr<AutofillMessageModel> message = CreateAndShowNewMessage();

  controller().OnActionClicked(message);

  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"Autofill.Message.", message->GetTypeAsString(), ".ActionClicked"}),
      true, 1);
}

TEST_F(AutofillMessageControllerTest, Metrics_OnDismissed) {
  base::HistogramTester histogram_tester;
  messages::DismissReason dismiss_reason =
      messages::DismissReason::PRIMARY_ACTION;
  raw_ptr<AutofillMessageModel> message = CreateAndShowNewMessage();
  std::string_view type_as_string = message->GetTypeAsString();

  controller().OnDismissed(message, dismiss_reason);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Autofill.Message.", type_as_string, ".Dismissed"}),
      dismiss_reason, 1);
}

}  // namespace autofill
