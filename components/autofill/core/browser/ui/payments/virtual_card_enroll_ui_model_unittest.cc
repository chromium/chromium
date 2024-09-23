// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

// Ensures default properties are set.
TEST(VirtualCardEnrollUiModelTest, CreateDefaultProperties) {
  std::unique_ptr<VirtualCardEnrollUiModel> model =
      std::make_unique<VirtualCardEnrollUiModel>(VirtualCardEnrollmentFields());

  EXPECT_EQ(model->window_title(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DIALOG_TITLE_LABEL));
  EXPECT_EQ(
      model->explanatory_message(),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DIALOG_CONTENT_LABEL,
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK_LABEL)));
  EXPECT_EQ(model->accept_action_text(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_ACCEPT_BUTTON_LABEL));
  EXPECT_EQ(model->learn_more_link_text(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK_LABEL));
}

struct CancelActionTextTestData {
  VirtualCardEnrollmentSource enrollment_source;
  bool last_show;
  int expected_string_resource;
};

class VirtualCardEnrollUiModelCancelActionTextTest
    : public testing::TestWithParam<CancelActionTextTestData> {};

// Parametrized test ensures the cancel_action_text is set.
TEST_P(VirtualCardEnrollUiModelCancelActionTextTest, CancelActionText) {
  CancelActionTextTestData config = GetParam();
  VirtualCardEnrollmentFields enrollment_fields;
  enrollment_fields.virtual_card_enrollment_source = config.enrollment_source;
  enrollment_fields.last_show = config.last_show;

  std::unique_ptr<VirtualCardEnrollUiModel> model =
      std::make_unique<VirtualCardEnrollUiModel>(enrollment_fields);

  EXPECT_EQ(model->cancel_action_text(),
            l10n_util::GetStringUTF16(config.expected_string_resource));
}

// Instantiates scenarios to test the cancel_action_text.
INSTANTIATE_TEST_SUITE_P(
    VirtualCardEnrollUiModelCancelActionTextTest,
    VirtualCardEnrollUiModelCancelActionTextTest,
    testing::Values(
        CancelActionTextTestData{
            .enrollment_source = VirtualCardEnrollmentSource::kDownstream,
            .last_show = true,
            .expected_string_resource =
                IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DECLINE_BUTTON_LABEL_NO_THANKS,
        },
        CancelActionTextTestData{
            .enrollment_source = VirtualCardEnrollmentSource::kDownstream,
            .last_show = false,
            .expected_string_resource =
                IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DECLINE_BUTTON_LABEL_SKIP,
        },
        CancelActionTextTestData{
            .enrollment_source = VirtualCardEnrollmentSource::kSettingsPage,
            .last_show = true,
            .expected_string_resource = IDS_CANCEL,
        },
        CancelActionTextTestData{
            .enrollment_source = VirtualCardEnrollmentSource::kSettingsPage,
            .last_show = false,
            .expected_string_resource = IDS_CANCEL,
        }));

// Ensures Create() copies the passed-in enrollment fields to the model.
TEST(VirtualCardEnrollUiModelEnrollmentFieldsTest, CopiesEnrollmentFields) {
  VirtualCardEnrollmentFields enrollment_fields;
  enrollment_fields.credit_card = autofill::test::GetCreditCard();
  enrollment_fields.google_legal_message =
      LegalMessageLines({TestLegalMessageLine("google_legal_message")});
  enrollment_fields.issuer_legal_message =
      LegalMessageLines({TestLegalMessageLine("issuer_legal_message")});

  std::unique_ptr<VirtualCardEnrollUiModel> model =
      std::make_unique<VirtualCardEnrollUiModel>(enrollment_fields);

  EXPECT_EQ(model->enrollment_fields(), enrollment_fields);
}

class MockVirtualCardEnrollUiModelObserver
    : public VirtualCardEnrollUiModel::Observer {
 public:
  MOCK_METHOD(
      void,
      OnEnrollmentProgressChanged,
      (VirtualCardEnrollUiModel::EnrollmentProgress enrollment_progress),
      (override));
};

class VirtualCardEnrollUiModelObserverTest : public ::testing::Test {
 public:
  void SetUp() override { model_->AddObserver(&observer_); }

  void TearDown() override { model_->RemoveObserver(&observer_); }

 protected:
  MockVirtualCardEnrollUiModelObserver observer_;
  std::unique_ptr<VirtualCardEnrollUiModel> model_ =
      std::make_unique<VirtualCardEnrollUiModel>(VirtualCardEnrollmentFields());
};

// Ensure enrollment progress notifies observers.
TEST_F(VirtualCardEnrollUiModelObserverTest,
       SetEnrollmentProgressNotifiesWhenChanged) {
  // This tests assumes the initial value is kOffered.
  ASSERT_EQ(model_->enrollment_progress(),
            VirtualCardEnrollUiModel::EnrollmentProgress::kOffered);

  EXPECT_CALL(observer_,
              OnEnrollmentProgressChanged(
                  VirtualCardEnrollUiModel::EnrollmentProgress::kEnrolled));

  // Set the enrollment progress to a different value.
  model_->SetEnrollmentProgress(
      VirtualCardEnrollUiModel::EnrollmentProgress::kEnrolled);
}

// Ensure enrollment progress notifies observers.
TEST_F(VirtualCardEnrollUiModelObserverTest,
       SetEnrollmentProgressDoesNotNotifyWhenUnchanged) {
  // This tests assumes the initial value is kOffered.
  ASSERT_EQ(model_->enrollment_progress(),
            VirtualCardEnrollUiModel::EnrollmentProgress::kOffered);

  EXPECT_CALL(observer_,
              OnEnrollmentProgressChanged(
                  VirtualCardEnrollUiModel::EnrollmentProgress::kOffered))
      .Times(0);

  // Set the enrollment progress to the same value.
  model_->SetEnrollmentProgress(
      VirtualCardEnrollUiModel::EnrollmentProgress::kOffered);
}

}  // namespace autofill
