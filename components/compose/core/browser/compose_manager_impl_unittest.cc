// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_manager_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/mock_autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/compose/core/browser/compose_client.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/config.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

using autofill::EqualsSuggestion;
using autofill::Suggestion;
using autofill::SuggestionType;
using ::testing::_;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using UiEntryPoint = autofill::AutofillComposeDelegate::UiEntryPoint;

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
              (const autofill::FormData& form,
               const autofill::FormFieldData& trigger_field,
               autofill::AutofillSuggestionTriggerSource trigger_source),
              (override));
  MOCK_METHOD(compose::PageUkmTracker*, GetPageUkmTracker, (), (override));
  MOCK_METHOD(void, DisableProactiveNudge, (), (override));
  MOCK_METHOD(void, OpenProactiveNudgeSettings, (), (override));
  MOCK_METHOD(void,
              AddSiteToNeverPromptList,
              (const url::Origin& origin),
              (override));
};

class MockAutofillDriver : public autofill::TestAutofillDriver {
 public:
  using autofill::TestAutofillDriver::TestAutofillDriver;
  MOCK_METHOD(void,
              ExtractForm,
              (autofill::FormGlobalId form,
               AutofillDriver::BrowserFormHandler response_handler),
              (override));
};

}  // namespace

class ComposeManagerImplTest : public testing::Test {
 public:
  void SetUp() override {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    const ukm::SourceId valid_test_source_id{1};
    page_ukm_tracker_ =
        std::make_unique<compose::PageUkmTracker>(valid_test_source_id);

    std::unique_ptr<testing::NiceMock<autofill::MockAutofillManager>>
        mock_autofill_manager =
            std::make_unique<testing::NiceMock<autofill::MockAutofillManager>>(
                &mock_autofill_driver_);
    mock_autofill_driver_.set_autofill_manager(
        std::move(mock_autofill_manager));

    // Needed for feature params to reset.
    compose::ResetConfigForTesting();

    // Allow the manager to obtain the PageUkmTracker instance.
    ON_CALL(mock_compose_client(), GetPageUkmTracker)
        .WillByDefault(testing::Return(page_ukm_tracker_.get()));
    // Record the FormFieldData sent to the client.
    ON_CALL(mock_compose_client(), ShowComposeDialog(_, _, _, _))
        .WillByDefault(testing::WithArg<1>(
            testing::Invoke([&](const autofill::FormFieldData& trigger_field) {
              last_form_field_to_client_ = trigger_field;
            })));
    ON_CALL(mock_compose_client(), ShouldTriggerPopup)
        .WillByDefault(testing::Return(true));
    compose_manager_impl_ =
        std::make_unique<compose::ComposeManagerImpl>(&mock_compose_client());
  }

  void TearDown() override {
    // Needed for feature params to reset.
    compose::ResetConfigForTesting();
  }

  // Helper method to retrieve compose suggestions, if it exists.
  // `has_session` defines whether a previous session exists for the triggering
  // field.
  std::optional<Suggestion> GetSuggestion(
      autofill::AutofillSuggestionTriggerSource trigger_source,
      bool has_session) {
    ON_CALL(mock_compose_client(), HasSession)
        .WillByDefault(testing::Return(has_session));
    return compose_manager_impl().GetSuggestion(
        autofill::FormData(),
        autofill::test::CreateTestFormField(
            "label0", "name0", "value0", autofill::FormControlType::kTextArea),
        trigger_source);
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
    form.set_url(GURL("https://www.foo.com"));
    form.set_fields(
        {autofill::test::CreateTestFormField(
             "label0", "name0", "value0", autofill::FormControlType::kTextArea),
         autofill::test::CreateTestFormField(
             "label1", "name1", "value1", autofill::FormControlType::kTextArea),
         autofill::test::CreateTestFormField(
             "label2", "name2", "value2",
             autofill::FormControlType::kTextArea)});
    return form;
  }

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry>
  GetUkmPageEntries(const std::vector<std::string>& metric_names) {
    return ukm_recorder_->GetEntries(
        ukm::builders::Compose_PageEvents::kEntryName, metric_names);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  testing::NiceMock<MockComposeClient> mock_compose_client_;
  autofill::TestAutofillClient test_autofill_client_;
  autofill::FormFieldData last_form_field_to_client_;
  testing::NiceMock<MockAutofillDriver> mock_autofill_driver_{
      &test_autofill_client_};
  std::unique_ptr<compose::PageUkmTracker> page_ukm_tracker_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<compose::ComposeManagerImpl> compose_manager_impl_;
};

TEST_F(
    ComposeManagerImplTest,
    SuggestionGeneration_HasSession_ComposeLostFocus_ApplyExpectedTextAndLabel) {
  std::optional<Suggestion> suggestion = GetSuggestion(
      autofill::AutofillSuggestionTriggerSource::kComposeDialogLostFocus,
      /*has_session=*/true);
  EXPECT_THAT(suggestion,
              Optional(EqualsSuggestion(
                  SuggestionType::kComposeSavedStateNotification,
                  l10n_util::GetStringUTF16(IDS_COMPOSE_SUGGESTION_SAVED_TEXT),
                  Suggestion::Icon::kPenSpark)));
}

TEST_F(
    ComposeManagerImplTest,
    SuggestionGeneration_ProactiveNudgeFeatureOn_DoesNotHaveSession_HasChildSuggestions) {
  base::test::ScopedFeatureList compose_proactive_nudge_feature{
      compose::features::kEnableComposeProactiveNudge};

  std::optional<Suggestion> suggestion = GetSuggestion(
      autofill::AutofillSuggestionTriggerSource::kComposeDialogLostFocus,
      /*has_session=*/false);
  ASSERT_TRUE(suggestion.has_value());
  // Checks that the 3 expected child suggestions exist.
  EXPECT_THAT(
      suggestion->children,
      ElementsAre(
          EqualsSuggestion(SuggestionType::kComposeNeverShowOnThisSiteAgain),
          EqualsSuggestion(SuggestionType::kComposeDisable),
          EqualsSuggestion(SuggestionType::kComposeGoToSettings)));
}

TEST_F(
    ComposeManagerImplTest,
    SuggestionGeneration_ProactiveNudgeFeatureOn_HasSession_NoChildSuggestions) {
  base::test::ScopedFeatureList compose_proactive_nudge_feature{
      compose::features::kEnableComposeProactiveNudge};

  std::optional<Suggestion> suggestion = GetSuggestion(
      autofill::AutofillSuggestionTriggerSource::kComposeDialogLostFocus,
      /*has_session=*/true);
  ASSERT_TRUE(suggestion.has_value());
  EXPECT_TRUE(suggestion->children.empty());
}

TEST_F(
    ComposeManagerImplTest,
    SuggestionGeneration_HasSession_ControlElementClicked_ApplyExpectedTextAndLabel) {
  std::optional<Suggestion> suggestion = GetSuggestion(
      autofill::AutofillSuggestionTriggerSource::kFormControlElementClicked,
      /*has_session=*/true);
  EXPECT_THAT(suggestion,
              Optional(EqualsSuggestion(
                  SuggestionType::kComposeResumeNudge,
                  l10n_util::GetStringUTF16(IDS_COMPOSE_SUGGESTION_SAVED_TEXT),
                  Suggestion::Icon::kPenSpark)));
}

TEST_F(ComposeManagerImplTest,
       SuggestionGeneration_NoSession_ExpectedTextAndLabel) {
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_compact_ui = false;
  std::optional<Suggestion> suggestion = GetSuggestion(
      autofill::AutofillSuggestionTriggerSource::kFormControlElementClicked,
      /*has_session=*/false);
  EXPECT_THAT(suggestion,
              Optional(EqualsSuggestion(
                  SuggestionType::kComposeProactiveNudge,
                  l10n_util::GetStringUTF16(IDS_COMPOSE_SUGGESTION_MAIN_TEXT),
                  Suggestion::Icon::kPenSpark,
                  {{autofill::Suggestion::Text(l10n_util::GetStringUTF16(
                      IDS_COMPOSE_SUGGESTION_LABEL))}})));
}

TEST_F(ComposeManagerImplTest,
       SuggestionGeneration_NoSession_CompactUI_ExpectedTextAndLabel) {
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_compact_ui = true;

  std::optional<Suggestion> suggestion = GetSuggestion(
      autofill::AutofillSuggestionTriggerSource::kFormControlElementClicked,
      /*has_session=*/false);
  EXPECT_THAT(suggestion,
              Optional(EqualsSuggestion(
                  SuggestionType::kComposeProactiveNudge,
                  l10n_util::GetStringUTF16(IDS_COMPOSE_SUGGESTION_MAIN_TEXT),
                  Suggestion::Icon::kPenSpark)));
}

TEST_F(ComposeManagerImplTest,
       SuggestionGeneration_ShouldNotTriggerPopup_NoSuggestionReturned) {
  ON_CALL(mock_compose_client(), ShouldTriggerPopup)
      .WillByDefault(testing::Return(false));
  std::optional<Suggestion> suggestion = GetSuggestion(
      autofill::AutofillSuggestionTriggerSource::kFormControlElementClicked,
      /*has_session=*/false);
  EXPECT_FALSE(suggestion.has_value());
}

TEST_F(ComposeManagerImplTest, TestOpenCompose_Success) {
  // Creates a test form and use the 2nd field as the selected one.
  const autofill::FormData form_data = CreateTestFormDataWith3TextAreaFields();
  const autofill::FormFieldData selected_form_field = form_data.fields()[1];

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

  auto ukm_entries = GetUkmPageEntries(
      {ukm::builders::Compose_PageEvents::kMenuItemClickedName,
       ukm::builders::Compose_PageEvents::kMissingFormDataName,
       ukm::builders::Compose_PageEvents::kMissingFormFieldDataName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_THAT(
      ukm_entries[0].metrics,
      UnorderedElementsAre(
          Pair(ukm::builders::Compose_PageEvents::kMenuItemClickedName, 1),
          Pair(ukm::builders::Compose_PageEvents::kMissingFormDataName, 0),
          Pair(ukm::builders::Compose_PageEvents::kMissingFormFieldDataName,
               0)));

  // Note: The success result is logged by the Compose client, not the manager.
  histograms().ExpectTotalCount(compose::kOpenComposeDialogResult, 0);
  histograms().ExpectUniqueSample(
      compose::kComposeContextMenuCtr,
      compose::ComposeContextMenuCtrEvent::kMenuItemClicked, 1);

  EXPECT_TRUE(selected_form_field.SameFieldAs(last_form_field_to_client()));
  EXPECT_EQ(last_form_field_to_client().selected_text(), u"value1");
}

TEST_F(ComposeManagerImplTest, TestOpenCompose_FormDataMissing) {
  // Creates form and field data only for having valid IDs.
  const autofill::FormData form_data = CreateTestFormDataWith3TextAreaFields();
  const autofill::FormFieldData selected_form_field = form_data.fields()[1];

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

  auto ukm_entries = GetUkmPageEntries(
      {ukm::builders::Compose_PageEvents::kMenuItemClickedName,
       ukm::builders::Compose_PageEvents::kMissingFormDataName,
       ukm::builders::Compose_PageEvents::kMissingFormFieldDataName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_THAT(
      ukm_entries[0].metrics,
      UnorderedElementsAre(
          Pair(ukm::builders::Compose_PageEvents::kMenuItemClickedName, 1),
          Pair(ukm::builders::Compose_PageEvents::kMissingFormDataName, 1),
          Pair(ukm::builders::Compose_PageEvents::kMissingFormFieldDataName,
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
  const autofill::FormFieldData selected_form_field = form_data.fields().back();
  test_api(form_data).Remove(-1);

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

  auto ukm_entries = GetUkmPageEntries(
      {ukm::builders::Compose_PageEvents::kMenuItemClickedName,
       ukm::builders::Compose_PageEvents::kMissingFormDataName,
       ukm::builders::Compose_PageEvents::kMissingFormFieldDataName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_THAT(
      ukm_entries[0].metrics,
      UnorderedElementsAre(
          Pair(ukm::builders::Compose_PageEvents::kMenuItemClickedName, 1),
          Pair(ukm::builders::Compose_PageEvents::kMissingFormDataName, 0),
          Pair(ukm::builders::Compose_PageEvents::kMissingFormFieldDataName,
               1)));

  histograms().ExpectUniqueSample(
      compose::kOpenComposeDialogResult,
      compose::OpenComposeDialogResult::kAutofillFormFieldDataNotFound, 1);
  histograms().ExpectUniqueSample(
      compose::kComposeContextMenuCtr,
      compose::ComposeContextMenuCtrEvent::kMenuItemClicked, 1);
}

TEST_F(ComposeManagerImplTest, NeverShowForOrigin_MetricsTest) {
  auto test_origin = url::Origin::Create(GURL("http://foo"));

  EXPECT_CALL(mock_compose_client(), AddSiteToNeverPromptList(test_origin));

  compose_manager_impl().NeverShowComposeForOrigin(test_origin);
  SimulateComposeSessionEnd();
}

TEST_F(ComposeManagerImplTest, DisableCompose_MetricTest) {
  EXPECT_CALL(mock_compose_client(), DisableProactiveNudge());

  compose_manager_impl().DisableCompose();
  SimulateComposeSessionEnd();
}

TEST_F(ComposeManagerImplTest, GoToSettings_HistogramTest) {
  EXPECT_CALL(mock_compose_client(), OpenProactiveNudgeSettings());
  compose_manager_impl().GoToSettings();
}
