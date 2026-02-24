// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_predictions_tracker.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/form_predictions_tracker_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class FormPredictionsTrackerTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<> {
 public:
  FormPredictionsTrackerTest() {
    InitAutofillClient();
    tracker_ = std::make_unique<FormPredictionsTracker>(&autofill_client());
    CreateAutofillDriver();
  }

  ~FormPredictionsTrackerTest() override { DestroyAutofillClient(); }

  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_feature_list_;
  }
  FormPredictionsTracker& tracker() { return *tracker_; }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillDelayApcForPredictions};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_unit_test_environment_;
  std::unique_ptr<FormPredictionsTracker> tracker_;
};

// Tests that when a new form is discovered (`OnBeforeFormsSeen`), it is added
// to the internal tracking map with both parsing bits initialized to false.
TEST_F(FormPredictionsTrackerTest, FormAddedOnSeen) {
  std::vector<FormGlobalId> forms = {test::MakeFormGlobalId()};
  const FormGlobalId& form_id = forms[0];

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen, forms,
      base::span<FormGlobalId>());

  const absl::flat_hash_map<
      FormGlobalId, FormPredictionsTracker::FormParsingStatus>& status_map =
      test_api(tracker()).form_parsing_status();
  ASSERT_TRUE(status_map.contains(form_id));
  EXPECT_FALSE(status_map.at(form_id).heuristic_parsed_in_actor_mode);
  EXPECT_FALSE(status_map.at(form_id).server_predicted_in_actor_mode);
}

// Tests that when a form is removed from the DOM (`OnBeforeFormsSeen` with
// removed_forms), it is successfully purged from the internal tracking map.
TEST_F(FormPredictionsTrackerTest, FormRemoved) {
  std::vector<FormGlobalId> forms = {test::MakeFormGlobalId()};

  // Add the form first.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen, forms,
      base::span<FormGlobalId>());
  ASSERT_EQ(test_api(tracker()).form_parsing_status().size(), 1u);

  // Notify that the form was removed.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen, base::span<FormGlobalId>(),
      forms);
  EXPECT_TRUE(test_api(tracker()).form_parsing_status().empty());
}

// Tests that when the AutofillManager's lifecycle state changes from active to
// inactive, all forms associated with that manager's frame are removed from
// tracking, while forms in other frames are preserved.
TEST_F(FormPredictionsTrackerTest, CleanupOnLifecycleChange) {
  LocalFrameToken frame_token = autofill_driver().GetFrameToken();
  FormGlobalId form_in_frame = {frame_token, FormRendererId(123)};

  LocalFrameToken other_frame_token(base::UnguessableToken::Create());
  FormGlobalId form_in_other_frame = {other_frame_token, FormRendererId(456)};

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form_in_frame, form_in_other_frame},
      base::span<FormGlobalId>());

  ASSERT_EQ(test_api(tracker()).form_parsing_status().size(), 2u);

  // Simulate the manager's frame becoming inactive.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAutofillManagerStateChanged,
      /*old_state=*/AutofillDriver::LifecycleState::kActive,
      /*new_state=*/AutofillDriver::LifecycleState::kPendingReset);

  // Verify only the form associated with the inactive frame was removed.
  const auto& status_map = test_api(tracker()).form_parsing_status();
  EXPECT_FALSE(status_map.contains(form_in_frame))
      << "Form in the deactivated frame should have been erased.";
  EXPECT_TRUE(status_map.contains(form_in_other_frame))
      << "Form in a different frame should still be tracked.";
}

// Tests that `OnAutofillManagerStateChanged`'s cleanup logic is NOT triggered
// when the state change does not involve transitioning away from kActive (e.g.,
// kInactive to kActive).
TEST_F(FormPredictionsTrackerTest, NoCleanupOnActivation) {
  LocalFrameToken frame_token = autofill_driver().GetFrameToken();
  FormGlobalId form_id = {frame_token, FormRendererId(123)};

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form_id}, base::span<FormGlobalId>());

  // Transition from kInactive to kActive should NOT trigger erasure.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAutofillManagerStateChanged,
      /*old_state=*/AutofillDriver::LifecycleState::kInactive,
      /*new_state=*/AutofillDriver::LifecycleState::kActive);

  EXPECT_TRUE(test_api(tracker()).form_parsing_status().contains(form_id));
}

// Tests the state transitions of a form. It verifies that heuristic and
// server parsing statuses are updated independently based on the source
// provided in `OnFieldTypesDetermined`.
TEST_F(FormPredictionsTrackerTest, StateTransitions) {
  std::vector<FormGlobalId> forms = {test::MakeFormGlobalId()};
  const FormGlobalId& form_id = forms[0];
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen, forms,
      base::span<FormGlobalId>());

  // Simulate local heuristic parsing completion.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form_id,
      AutofillManager::Observer::FieldTypeSource::kHeuristicsOrAutocomplete,
      /*small_forms_were_parsed=*/true);
  EXPECT_TRUE(test_api(tracker())
                  .form_parsing_status()
                  .at(form_id)
                  .heuristic_parsed_in_actor_mode);
  EXPECT_FALSE(test_api(tracker())
                   .form_parsing_status()
                   .at(form_id)
                   .server_predicted_in_actor_mode);

  // Simulate server-side parsing completion.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form_id,
      AutofillManager::Observer::FieldTypeSource::kAutofillServer,
      /*small_forms_were_parsed=*/true);
  EXPECT_TRUE(test_api(tracker())
                  .form_parsing_status()
                  .at(form_id)
                  .server_predicted_in_actor_mode);
}

// Tests that if a form is seen again (e.g., the DOM was modified and
// re-triggered `OnBeforeFormsSeen`), its parsing status is reset to false
// because the previous parsing results may no longer be valid.
TEST_F(FormPredictionsTrackerTest, ResetOnReSeen) {
  std::vector<FormGlobalId> forms = {test::MakeFormGlobalId()};
  const FormGlobalId& form_id = forms[0];
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen, forms,
      base::span<FormGlobalId>());

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form_id,
      AutofillManager::Observer::FieldTypeSource::kHeuristicsOrAutocomplete,
      /*small_forms_were_parsed=*/true);

  // Simulate the same form being updated/re-processed by the manager.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen, forms,
      base::span<FormGlobalId>());

  // The status should be cleared (reset to default).
  EXPECT_FALSE(test_api(tracker())
                   .form_parsing_status()
                   .at(form_id)
                   .heuristic_parsed_in_actor_mode);
}

// Tests that the detector respects the `small_forms_were_parsed` flag. If the
// manager determines types but small forms were NOT parsed, the detector
// should not mark the form as parsed.
TEST_F(FormPredictionsTrackerTest, IgnoreSmallForms) {
  std::vector<FormGlobalId> forms = {test::MakeFormGlobalId()};
  const FormGlobalId& form_id = forms[0];
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen, forms,
      base::span<FormGlobalId>());

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form_id,
      AutofillManager::Observer::FieldTypeSource::kHeuristicsOrAutocomplete,
      /*small_forms_were_parsed=*/false);

  EXPECT_FALSE(test_api(tracker())
                   .form_parsing_status()
                   .at(form_id)
                   .heuristic_parsed_in_actor_mode);
}

// Tests that if no forms are currently tracked, calling Wait() executes the
// callback immediately.
TEST_F(FormPredictionsTrackerTest, Wait_ExecutesImmediatelyIfNoForms) {
  base::test::TestFuture<void> future;
  tracker().Wait(future.GetCallback(), base::Milliseconds(1000));
  EXPECT_TRUE(future.Wait());
}

// Tests that if Wait() is called when all tracked forms are already fully
// parsed, the callback is executed immediately.
TEST_F(FormPredictionsTrackerTest, Wait_ExecutesImmediatelyIfAlreadyParsed) {
  FormGlobalId form_id = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form_id}, base::span<FormGlobalId>());

  // Fully parse the form before calling Wait.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form_id,
      AutofillManager::Observer::FieldTypeSource::kHeuristicsOrAutocomplete,
      true);
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form_id,
      AutofillManager::Observer::FieldTypeSource::kAutofillServer, true);

  base::test::TestFuture<void> future;
  tracker().Wait(future.GetCallback(), base::Milliseconds(1000));
  EXPECT_TRUE(future.Wait());
}

// Tests that if a form is tracked but not fully parsed, Wait() defers the
// callback until parsing is complete.
TEST_F(FormPredictionsTrackerTest, Wait_DefersUntilFormFullyParsed) {
  FormGlobalId form_id = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form_id}, base::span<FormGlobalId>());

  base::test::TestFuture<void> future;
  tracker().Wait(future.GetCallback(), base::Milliseconds(1000));

  // Finish heuristic parsing.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form_id,
      AutofillManager::Observer::FieldTypeSource::kHeuristicsOrAutocomplete,
      /*small_forms_were_parsed=*/true);
  EXPECT_FALSE(future.IsReady());

  // Finish server parsing, callback should be executed.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form_id,
      AutofillManager::Observer::FieldTypeSource::kAutofillServer,
      /*small_forms_were_parsed=*/true);
  EXPECT_TRUE(future.Wait());
}

// Tests that if multiple forms are tracked, Wait() waits for the last remaining
// parsing bit across all forms.
TEST_F(FormPredictionsTrackerTest, Wait_UntilMultipleFormsParsed) {
  FormGlobalId form1 = test::MakeFormGlobalId();
  FormGlobalId form2 = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form1, form2}, base::span<FormGlobalId>());

  base::test::TestFuture<void> future;
  tracker().Wait(future.GetCallback(), base::Milliseconds(1000));

  // Fully parse form 1.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form1,
      AutofillManager::Observer::FieldTypeSource::kHeuristicsOrAutocomplete,
      true);
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form1,
      AutofillManager::Observer::FieldTypeSource::kAutofillServer, true);
  EXPECT_FALSE(future.IsReady());

  // Form 2 heuristics done.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form2,
      AutofillManager::Observer::FieldTypeSource::kHeuristicsOrAutocomplete,
      true);
  EXPECT_FALSE(future.IsReady());

  // Form 2 fully done, callback should be executed.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form2,
      AutofillManager::Observer::FieldTypeSource::kAutofillServer, true);
  EXPECT_TRUE(future.Wait());
}

// Tests that calling Wait() while a callback is already registered, schedules
// another callback, all of which will be executed once requirements are met.
TEST_F(FormPredictionsTrackerTest, Wait_MultipleCallbacksPending) {
  FormGlobalId form_id = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form_id}, base::span<FormGlobalId>());

  base::test::TestFuture<void> future1;
  base::test::TestFuture<void> future2;

  tracker().Wait(future1.GetCallback(), base::Milliseconds(1000));
  tracker().Wait(future2.GetCallback(), base::Milliseconds(1000));
  EXPECT_EQ(2UL, test_api(tracker()).num_callbacks());

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form_id,
      AutofillManager::Observer::FieldTypeSource::kHeuristicsOrAutocomplete,
      true);
  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  // Both callbacks should be executed when requirements are met.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form_id,
      AutofillManager::Observer::FieldTypeSource::kAutofillServer, true);

  EXPECT_TRUE(future1.Wait());
  EXPECT_TRUE(future2.Wait());
  EXPECT_EQ(0UL, test_api(tracker()).num_callbacks());
}

// Tests that the tracker can be reused. Once a callback has been executed,
// a subsequent call to Wait() should successfully register a new callback
// and wait for new forms to complete.
TEST_F(FormPredictionsTrackerTest, Wait_ReschedulesAfterExecution) {
  FormGlobalId form1 = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form1}, base::span<FormGlobalId>());

  base::test::TestFuture<void> future1;
  tracker().Wait(future1.GetCallback(), base::Milliseconds(1000));

  // Fully parse form 1 to fire the first callback.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form1,
      AutofillManager::Observer::FieldTypeSource::kHeuristicsOrAutocomplete,
      true);
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form1,
      AutofillManager::Observer::FieldTypeSource::kAutofillServer, true);
  EXPECT_TRUE(future1.Wait());

  // The second form is added, the tracker should now be in an "unparsed" state
  // again.
  FormGlobalId form2 = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form2}, base::span<FormGlobalId>());

  // Register a second wait.
  base::test::TestFuture<void> future2;
  tracker().Wait(future2.GetCallback(), base::Milliseconds(1000));
  EXPECT_FALSE(future2.IsReady());

  // Form 2 gets fully parsed, the future2 should be ready.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form2,
      AutofillManager::Observer::FieldTypeSource::kHeuristicsOrAutocomplete,
      true);
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form2,
      AutofillManager::Observer::FieldTypeSource::kAutofillServer, true);
  EXPECT_TRUE(future2.Wait());
}

// Verifies that timeouts set when waiting are respected and the callback gets
// automatically executed even if requirements are not met.
TEST_F(FormPredictionsTrackerTest, Wait_TimeoutOnSingleCallback) {
  FormGlobalId form = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form}, base::span<FormGlobalId>());

  base::test::TestFuture<void> future;
  tracker().Wait(future.GetCallback(), base::Milliseconds(500));
  EXPECT_FALSE(future.IsReady());

  task_environment().FastForwardBy(base::Milliseconds(100));
  EXPECT_FALSE(future.IsReady());

  task_environment().FastForwardBy(base::Milliseconds(400));
  EXPECT_TRUE(future.Wait());
}

// Verifies that if the callback got executed because of a timeout, it is not
// executed again after the requirements are met.
TEST_F(FormPredictionsTrackerTest,
       Wait_RequirementsMetAfterTimeoutSingleCallback) {
  FormGlobalId form = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form}, base::span<FormGlobalId>());

  base::test::TestFuture<void> future;
  tracker().Wait(future.GetCallback(), base::Milliseconds(500));
  EXPECT_FALSE(future.IsReady());

  task_environment().FastForwardBy(base::Milliseconds(500));
  EXPECT_TRUE(future.Wait());

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form,
      AutofillManager::Observer::FieldTypeSource::kHeuristicsOrAutocomplete,
      true);
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form,
      AutofillManager::Observer::FieldTypeSource::kAutofillServer, true);
}

// Verifies that timeouts are handled correctly even if multiple callbacks are
// pending.
TEST_F(FormPredictionsTrackerTest, Wait_TimeoutsOnMultipleCallbacksPending) {
  FormGlobalId form = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form}, base::span<FormGlobalId>());

  base::test::TestFuture<void> future1;
  base::test::TestFuture<void> future2;
  base::test::TestFuture<void> future3;

  tracker().Wait(future1.GetCallback(), base::Milliseconds(750));
  tracker().Wait(future2.GetCallback(), base::Milliseconds(500));
  tracker().Wait(future3.GetCallback(), base::Milliseconds(1000));

  task_environment().FastForwardBy(base::Milliseconds(500));
  EXPECT_FALSE(future1.IsReady());
  EXPECT_TRUE(future2.Wait());
  EXPECT_FALSE(future3.IsReady());

  task_environment().FastForwardBy(base::Milliseconds(250));
  EXPECT_TRUE(future1.Wait());
  EXPECT_FALSE(future3.IsReady());

  task_environment().FastForwardBy(base::Milliseconds(250));
  EXPECT_TRUE(future3.Wait());
}

// Verifies that one callback timing out doesn't block other callbacks from
// being executed as a result of requirements being met.
TEST_F(FormPredictionsTrackerTest,
       Wait_RequirementsMetMultipleCallbacksPendingOneTimedout) {
  FormGlobalId form = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form}, base::span<FormGlobalId>());

  base::test::TestFuture<void> future1;
  base::test::TestFuture<void> future2;
  base::test::TestFuture<void> future3;

  tracker().Wait(future1.GetCallback(), base::Milliseconds(1000));
  tracker().Wait(future2.GetCallback(), base::Milliseconds(500));
  tracker().Wait(future3.GetCallback(), base::Milliseconds(1000));

  task_environment().FastForwardBy(base::Milliseconds(500));
  EXPECT_FALSE(future1.IsReady());
  EXPECT_TRUE(future2.Wait());
  EXPECT_FALSE(future3.IsReady());

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form,
      AutofillManager::Observer::FieldTypeSource::kHeuristicsOrAutocomplete,
      true);
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined, form,
      AutofillManager::Observer::FieldTypeSource::kAutofillServer, true);
  EXPECT_TRUE(future1.Wait());
  EXPECT_TRUE(future3.Wait());
}

TEST_F(FormPredictionsTrackerTest, Wait_FeatureDisabled) {
  scoped_feature_list().Reset();
  scoped_feature_list().InitAndDisableFeature(
      features::kAutofillDelayApcForPredictions);

  FormGlobalId form_id = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form_id}, base::span<FormGlobalId>());

  // Since the flag is disabled, there should be no waiting.
  base::test::TestFuture<void> future;
  tracker().Wait(future.GetCallback(), base::Milliseconds(1000));
  EXPECT_TRUE(future.Wait());
}

// Tests that if a form is reported in `OnAfterFormsSeen` but has no fields,
// it is removed from the tracking map.
TEST_F(FormPredictionsTrackerTest, EmptyFormRemovedAfterSeen) {
  FormData form_data;
  form_data.set_renderer_id(test::MakeFormRendererId());
  // No fields added to form_data.

  autofill_manager().OnFormsSeen({form_data}, {});
  FormGlobalId form_id = form_data.global_id();

  // The manager should now have a FormStructure with 0 fields.
  // FormPredictionsTracker::OnAfterFormsSeen is triggered by OnFormsSeen.
  // Since the form has 0 fields, it should be erased.
  EXPECT_FALSE(test_api(tracker()).form_parsing_status().contains(form_id));
}

// Tests that OnFieldTypesDetermined does nothing if the form is no longer
// being tracked (e.g., it was removed from the DOM).
TEST_F(FormPredictionsTrackerTest, OnFieldTypesDetermined_NoOpsIfFormMissing) {
  FormGlobalId form_id = test::MakeFormGlobalId();
  ASSERT_FALSE(autofill_manager().FindCachedFormById(form_id));

  // This should not crash and should not add the form to the map.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFieldTypesDetermined,
      test::MakeFormGlobalId(),
      AutofillManager::Observer::FieldTypeSource::kAutofillServer, true);
  EXPECT_FALSE(test_api(tracker()).form_parsing_status().contains(form_id));
}

}  // namespace

}  // namespace autofill
