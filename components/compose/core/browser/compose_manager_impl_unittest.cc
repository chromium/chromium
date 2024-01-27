// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/compose/core/browser/compose_client.h"
#include "components/compose/core/browser/compose_manager_impl.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using UiEntryPoint = autofill::AutofillComposeDelegate::UiEntryPoint;

namespace {

class MockComposeClient : public compose::ComposeClient {
 public:
  MOCK_METHOD(compose::ComposeManager&, GetManager, (), (override));
  MOCK_METHOD(bool,
              HasSession,
              (const autofill::FieldGlobalId& trigger_field_id),
              (override));
  MOCK_METHOD(void,
              ShowComposeDialog,
              (UiEntryPoint ui_entry_point,
               const autofill::FormFieldData& trigger_field,
               std::optional<autofill::AutofillClient::PopupScreenLocation>
                   popup_screen_location,
               ComposeCallback callback),
              (override));
  MOCK_METHOD(bool,
              ShouldTriggerPopup,
              (const autofill::FormFieldData& trigger_field),
              (override));
  MOCK_METHOD(compose::PageUkmTracker*, getPageUkmTracker, (), (override));
};

class MockAutofillDriver : public autofill::TestAutofillDriver {
 public:
  MOCK_METHOD(void,
              ExtractForm,
              (autofill::FormGlobalId form,
               AutofillDriver::BrowserFormHandler response_handler),
              (override));
};

// TODO(b/318841248): deduplicate with the mock in autofill_manager_unittest.cc.
class MockAutofillManager : public autofill::AutofillManager {
 public:
  MockAutofillManager(autofill::AutofillDriver* driver,
                      autofill::AutofillClient* client)
      : autofill::AutofillManager(driver, client) {}

  base::WeakPtr<autofill::AutofillManager> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  MOCK_METHOD(bool, ShouldClearPreviewedForm, (), (override));
  MOCK_METHOD(void,
              OnFocusNoLongerOnFormImpl,
              (bool had_interacted_form),
              (override));
  MOCK_METHOD(void,
              OnDidFillAutofillFormDataImpl,
              (const autofill::FormData& form, const base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(void, OnDidEndTextFieldEditingImpl, (), (override));
  MOCK_METHOD(void, OnHidePopupImpl, (), (override));
  MOCK_METHOD(void,
              OnSelectOrSelectListFieldOptionsDidChangeImpl,
              (const autofill::FormData& form),
              (override));
  MOCK_METHOD(void,
              OnJavaScriptChangedAutofilledValueImpl,
              (const autofill::FormData& form,
               const autofill::FormFieldData& field,
               const std::u16string& old_value),
              (override));
  MOCK_METHOD(void,
              OnFormSubmittedImpl,
              (const autofill::FormData& form,
               bool known_success,
               autofill::mojom::SubmissionSource source),
              (override));
  MOCK_METHOD(void,
              OnTextFieldDidChangeImpl,
              (const autofill::FormData& form,
               const autofill::FormFieldData& field,
               const gfx::RectF& bounding_box,
               const base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(void,
              OnTextFieldDidScrollImpl,
              (const autofill::FormData& form,
               const autofill::FormFieldData& field,
               const gfx::RectF& bounding_box),
              (override));
  MOCK_METHOD(void,
              OnAskForValuesToFillImpl,
              (const autofill::FormData& form,
               const autofill::FormFieldData& field,
               const gfx::RectF& bounding_box,
               autofill::AutofillSuggestionTriggerSource trigger_source),
              (override));
  MOCK_METHOD(void,
              OnFocusOnFormFieldImpl,
              (const autofill::FormData& form,
               const autofill::FormFieldData& field,
               const gfx::RectF& bounding_box),
              (override));
  MOCK_METHOD(void,
              OnSelectControlDidChangeImpl,
              (const autofill::FormData& form,
               const autofill::FormFieldData& field,
               const gfx::RectF& bounding_box),
              (override));
  MOCK_METHOD(bool, ShouldParseForms, (), (override));
  MOCK_METHOD(void, OnBeforeProcessParsedForms, (), (override));
  MOCK_METHOD(void,
              OnFormProcessed,
              (const autofill::FormData& form_data,
               const autofill::FormStructure& form_structure),
              (override));
  MOCK_METHOD(void,
              OnAfterProcessParsedForms,
              (const autofill::DenseSet<autofill::FormType>& form_types),
              (override));
  MOCK_METHOD(void,
              ReportAutofillWebOTPMetrics,
              (bool used_web_otp),
              (override));
  MOCK_METHOD(void,
              OnContextMenuShownInField,
              (const autofill::FormGlobalId& form_global_id,
               const autofill::FieldGlobalId& field_global_id),
              (override));

 private:
  base::WeakPtrFactory<MockAutofillManager> weak_ptr_factory_{this};
};

}  // namespace

class ComposeManagerImplTest : public testing::Test {
 public:
  void SetUp() override {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    const ukm::SourceId valid_test_source_id{1};
    page_ukm_tracker_ =
        std::make_unique<compose::PageUkmTracker>(valid_test_source_id);

    std::unique_ptr<testing::NiceMock<MockAutofillManager>>
        mock_autofill_manager =
            std::make_unique<testing::NiceMock<MockAutofillManager>>(
                &mock_autofill_driver_, &test_autofill_client_);
    mock_autofill_driver_.set_autofill_manager(
        std::move(mock_autofill_manager));

    // Allow the manager to obtain the PageUkmTracker instance.
    ON_CALL(mock_compose_client(), getPageUkmTracker)
        .WillByDefault(testing::Return(page_ukm_tracker_.get()));
    // Record the FormFieldData sent to the client.
    ON_CALL(mock_compose_client(), ShowComposeDialog(_, _, _, _))
        .WillByDefault(testing::WithArg<1>(
            testing::Invoke([&](const autofill::FormFieldData& trigger_field) {
              last_form_field_to_client_ = trigger_field;
            })));

    compose_manager_impl_ =
        std::make_unique<compose::ComposeManagerImpl>(&mock_compose_client());
  }

  void SimulateComposeSessionEnd() { page_ukm_tracker_.reset(); }

 protected:
  compose::ComposeManagerImpl& compose_manager_impl() {
    return *compose_manager_impl_;
  }
  MockComposeClient& mock_compose_client() { return mock_compose_client_; }
  MockAutofillDriver& mock_autofill_driver() { return mock_autofill_driver_; }
  const base::HistogramTester& histograms() const { return histogram_tester_; }
  const autofill::FormFieldData& last_form_field_to_client() const {
    return last_form_field_to_client_;
  }

  autofill::FormData CreateTestFormDataWith3TextAreaFields() {
    autofill::FormData form;
    form.url = GURL("https://www.foo.com");
    form.fields = {
        autofill::test::CreateTestFormField(
            "label0", "name0", "value0", autofill::FormControlType::kTextArea),
        autofill::test::CreateTestFormField(
            "label1", "name1", "value1", autofill::FormControlType::kTextArea),
        autofill::test::CreateTestFormField(
            "label2", "name2", "value2", autofill::FormControlType::kTextArea)};
    return form;
  }

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry>
  GetUkmPageEntries() {
    return ukm_recorder_->GetEntries(
        ukm::builders::Compose_PageEvents::kEntryName,
        {ukm::builders::Compose_PageEvents::kMenuItemClickedName,
         ukm::builders::Compose_PageEvents::kMissingFormDataName,
         ukm::builders::Compose_PageEvents::kMissingFormFieldDataName});
  }

 private:
  base::test::TaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  testing::NiceMock<MockComposeClient> mock_compose_client_;
  autofill::TestAutofillClient test_autofill_client_;
  autofill::FormFieldData last_form_field_to_client_;
  testing::NiceMock<MockAutofillDriver> mock_autofill_driver_;
  std::unique_ptr<compose::PageUkmTracker> page_ukm_tracker_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<compose::ComposeManagerImpl> compose_manager_impl_;
};

TEST_F(ComposeManagerImplTest, TestOpenCompose_Success) {
  // Creates a test form and use the 2nd field as the selected one.
  const autofill::FormData form_data = CreateTestFormDataWith3TextAreaFields();
  const autofill::FormFieldData selected_form_field = form_data.fields[1];

  // Emulates the expected Autofill driver response.
  EXPECT_CALL(mock_autofill_driver(), ExtractForm(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](autofill::AutofillDriver::BrowserFormHandler callback) {
            std::move(callback).Run(&mock_autofill_driver(), form_data);
          })));

  const UiEntryPoint ui_entry_point = UiEntryPoint::kContextMenu;
  EXPECT_CALL(
      mock_compose_client(),
      ShowComposeDialog(/*ui_entry_point=*/ui_entry_point, /*trigger_field=*/_,
                        /*popup_screen_location=*/_, /*callback=*/_));

  base::RunLoop run_loop;
  compose_manager_impl().OpenCompose(
      mock_autofill_driver(), form_data.global_id(),
      selected_form_field.global_id(), ui_entry_point);
  run_loop.RunUntilIdle();
  SimulateComposeSessionEnd();

  auto ukm_entries = GetUkmPageEntries();
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_THAT(
      ukm_entries[0].metrics,
      testing::UnorderedElementsAre(
          testing::Pair(ukm::builders::Compose_PageEvents::kMenuItemClickedName,
                        1),
          testing::Pair(ukm::builders::Compose_PageEvents::kMissingFormDataName,
                        0),
          testing::Pair(
              ukm::builders::Compose_PageEvents::kMissingFormFieldDataName,
              0)));

  // Note: The success result is logged by the Compose client, not the manager.
  histograms().ExpectTotalCount(compose::kOpenComposeDialogResult, 0);
  histograms().ExpectUniqueSample(
      compose::kComposeContextMenuCtr,
      compose::ComposeContextMenuCtrEvent::kMenuItemClicked, 1);

  EXPECT_TRUE(selected_form_field.SameFieldAs(last_form_field_to_client()));
}

TEST_F(ComposeManagerImplTest, TestOpenCompose_FormDataMissing) {
  // Creates form and field data only for having valid IDs.
  const autofill::FormData form_data = CreateTestFormDataWith3TextAreaFields();
  const autofill::FormFieldData selected_form_field = form_data.fields[1];

  // Autofill driver returns no FormData.
  EXPECT_CALL(mock_autofill_driver(), ExtractForm(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](autofill::AutofillDriver::BrowserFormHandler callback) {
            std::move(callback).Run(&mock_autofill_driver(), std::nullopt);
          })));
  // There should be no attempt to open the dialog.
  EXPECT_CALL(mock_compose_client(),
              ShowComposeDialog(/*ui_entry_point=*/_, /*trigger_field=*/_,
                                /*popup_screen_location=*/_, /*callback=*/_))
      .Times(0);

  base::RunLoop run_loop;
  compose_manager_impl().OpenCompose(
      mock_autofill_driver(), form_data.global_id(),
      selected_form_field.global_id(), UiEntryPoint::kContextMenu);
  run_loop.RunUntilIdle();
  SimulateComposeSessionEnd();

  auto ukm_entries = GetUkmPageEntries();
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_THAT(
      ukm_entries[0].metrics,
      testing::UnorderedElementsAre(
          testing::Pair(ukm::builders::Compose_PageEvents::kMenuItemClickedName,
                        1),
          testing::Pair(ukm::builders::Compose_PageEvents::kMissingFormDataName,
                        1),
          testing::Pair(
              ukm::builders::Compose_PageEvents::kMissingFormFieldDataName,
              0)));

  histograms().ExpectUniqueSample(
      compose::kOpenComposeDialogResult,
      compose::OpenComposeDialogResult::kAutofillFormDataNotFound, 1);
  histograms().ExpectUniqueSample(
      compose::kComposeContextMenuCtr,
      compose::ComposeContextMenuCtrEvent::kMenuItemClicked, 1);
}

TEST_F(ComposeManagerImplTest, TestOpenCompose_FormFieldDataMissing) {
  // Creates a form and removes the last element, whose now unlisted ID is used.
  autofill::FormData form_data = CreateTestFormDataWith3TextAreaFields();
  const autofill::FormFieldData selected_form_field = form_data.fields.back();
  form_data.fields.pop_back();

  // Emulates the expected Autofill driver response.
  EXPECT_CALL(mock_autofill_driver(), ExtractForm(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](autofill::AutofillDriver::BrowserFormHandler callback) {
            std::move(callback).Run(&mock_autofill_driver(), form_data);
          })));
  // There should be no attempt to open the dialog.
  EXPECT_CALL(mock_compose_client(),
              ShowComposeDialog(/*ui_entry_point=*/_, /*trigger_field=*/_,
                                /*popup_screen_location=*/_, /*callback=*/_))
      .Times(0);

  base::RunLoop run_loop;
  compose_manager_impl().OpenCompose(
      mock_autofill_driver(), form_data.global_id(),
      selected_form_field.global_id(), UiEntryPoint::kContextMenu);
  run_loop.RunUntilIdle();
  SimulateComposeSessionEnd();

  auto ukm_entries = GetUkmPageEntries();
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_THAT(
      ukm_entries[0].metrics,
      testing::UnorderedElementsAre(
          testing::Pair(ukm::builders::Compose_PageEvents::kMenuItemClickedName,
                        1),
          testing::Pair(ukm::builders::Compose_PageEvents::kMissingFormDataName,
                        0),
          testing::Pair(
              ukm::builders::Compose_PageEvents::kMissingFormFieldDataName,
              1)));

  histograms().ExpectUniqueSample(
      compose::kOpenComposeDialogResult,
      compose::OpenComposeDialogResult::kAutofillFormFieldDataNotFound, 1);
  histograms().ExpectUniqueSample(
      compose::kComposeContextMenuCtr,
      compose::ComposeContextMenuCtrEvent::kMenuItemClicked, 1);
}
