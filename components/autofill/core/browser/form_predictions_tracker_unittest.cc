// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_predictions_tracker.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/form_predictions_tracker_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/mock_autofill_manager_observer.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
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
    autofill_client().set_is_tab_in_actor_mode(true);
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

// Tests that when forms are seen (`OnBeforeFormsSeen`), they are added to the
// parsing state set, and when `OnAfterFormsSeen` runs, both updated and removed
// forms are purged from the set.
TEST_F(FormPredictionsTrackerTest, FormsSeenState) {
  std::vector<FormGlobalId> updated_forms = {test::MakeFormGlobalId()};
  std::vector<FormGlobalId> removed_forms = {test::MakeFormGlobalId()};

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen, updated_forms,
      removed_forms);
  EXPECT_TRUE(
      test_api(tracker()).forms_in_parsing_state().contains(updated_forms[0]));

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAfterFormsSeen, updated_forms,
      removed_forms);
  EXPECT_TRUE(test_api(tracker()).forms_in_parsing_state().empty());
}

// Tests that when server predictions are queried
// (`OnBeforeLoadedServerPredictions`), forms are added to the awaiting response
// set, and when `OnAfterLoadedServerPredictions` runs, they are purged.
TEST_F(FormPredictionsTrackerTest, ServerPredictionsState) {
  std::vector<FormGlobalId> forms = {test::MakeFormGlobalId()};

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeLoadedServerPredictions, forms);
  EXPECT_TRUE(
      test_api(tracker()).forms_awaiting_server_response().contains(forms[0]));

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAfterLoadedServerPredictions, forms);
  EXPECT_TRUE(test_api(tracker()).forms_awaiting_server_response().empty());
}

// Tests that when `OnAfterFormsSeen` runs with removed forms, those forms are
// purged from `forms_awaiting_server_response` as well.
TEST_F(FormPredictionsTrackerTest, RemovedFormsPurgedFromServerResponseSet) {
  std::vector<FormGlobalId> forms = {test::MakeFormGlobalId()};

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeLoadedServerPredictions, forms);
  EXPECT_TRUE(
      test_api(tracker()).forms_awaiting_server_response().contains(forms[0]));

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      /*updated_forms=*/base::span<FormGlobalId>(),
      /*removed_forms=*/forms);
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAfterFormsSeen,
      /*updated_forms=*/base::span<FormGlobalId>(),
      /*removed_forms=*/forms);
  EXPECT_TRUE(test_api(tracker()).forms_awaiting_server_response().empty());
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
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeLoadedServerPredictions,
      std::vector<FormGlobalId>{form_in_frame, form_in_other_frame});

  ASSERT_EQ(test_api(tracker()).forms_in_parsing_state().size(), 2u);
  ASSERT_EQ(test_api(tracker()).forms_awaiting_server_response().size(), 2u);

  // Simulate the manager's frame becoming inactive.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAutofillManagerStateChanged,
      /*old_state=*/AutofillDriver::LifecycleState::kActive,
      /*new_state=*/AutofillDriver::LifecycleState::kPendingReset);

  // Verify only the form associated with the inactive frame was removed.
  const auto& parsing_set = test_api(tracker()).forms_in_parsing_state();
  EXPECT_FALSE(parsing_set.contains(form_in_frame))
      << "Form in the deactivated frame should have been erased.";
  EXPECT_TRUE(parsing_set.contains(form_in_other_frame))
      << "Form in a different frame should still be tracked.";

  const auto& response_set =
      test_api(tracker()).forms_awaiting_server_response();
  EXPECT_FALSE(response_set.contains(form_in_frame));
  EXPECT_TRUE(response_set.contains(form_in_other_frame));
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

  EXPECT_TRUE(test_api(tracker()).forms_in_parsing_state().contains(form_id));
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
      &AutofillManager::Observer::OnAfterFormsSeen,
      std::vector<FormGlobalId>{form_id}, base::span<FormGlobalId>());

  base::test::TestFuture<void> future;
  tracker().Wait(future.GetCallback(), base::Milliseconds(1000));
  EXPECT_TRUE(future.Wait());
}

// Tests that if a form is seen (`OnBeforeFormsSeen`) but never triggers a
// server query, calling `OnAfterFormsSeen` is sufficient to execute `Wait()`.
TEST_F(FormPredictionsTrackerTest, Wait_NoServerQuery) {
  FormGlobalId form_id = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form_id}, base::span<FormGlobalId>());

  base::test::TestFuture<void> future;
  tracker().Wait(future.GetCallback(), base::Milliseconds(1000));
  EXPECT_FALSE(future.IsReady());

  // Form completes local parsing without initiating a server query.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAfterFormsSeen,
      std::vector<FormGlobalId>{form_id}, base::span<FormGlobalId>());
  EXPECT_TRUE(future.Wait());
}

// Tests that if a form is tracked for both local parsing and server
// predictions, Wait() defers the callback until both have completed.
TEST_F(FormPredictionsTrackerTest, Wait_DefersUntilFormFullyParsed) {
  FormGlobalId form_id = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form_id}, base::span<FormGlobalId>());
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeLoadedServerPredictions,
      std::vector<FormGlobalId>{form_id});

  base::test::TestFuture<void> future;
  tracker().Wait(future.GetCallback(), base::Milliseconds(1000));

  // Local form parsing finishes first.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAfterFormsSeen,
      std::vector<FormGlobalId>{form_id}, base::span<FormGlobalId>());
  EXPECT_FALSE(future.IsReady());

  // Server predictions loading finishes second, callback should be executed.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAfterLoadedServerPredictions,
      std::vector<FormGlobalId>{form_id});
  EXPECT_TRUE(future.Wait());
}

// Tests that if server predictions complete before local parsing, Wait() defers
// the callback until local parsing finishes.
TEST_F(FormPredictionsTrackerTest,
       Wait_DefersUntilFormFullyParsed_ServerFirst) {
  FormGlobalId form_id = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form_id}, base::span<FormGlobalId>());
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeLoadedServerPredictions,
      std::vector<FormGlobalId>{form_id});

  base::test::TestFuture<void> future;
  tracker().Wait(future.GetCallback(), base::Milliseconds(1000));

  // Server predictions loading finishes first.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAfterLoadedServerPredictions,
      std::vector<FormGlobalId>{form_id});
  EXPECT_FALSE(future.IsReady());

  // Local form parsing finishes second, callback should be executed.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAfterFormsSeen,
      std::vector<FormGlobalId>{form_id}, base::span<FormGlobalId>());
  EXPECT_TRUE(future.Wait());
}

// Tests that if multiple forms are tracked, Wait() waits for the last remaining
// parsing state across all forms.
TEST_F(FormPredictionsTrackerTest, Wait_UntilMultipleFormsParsed) {
  FormGlobalId form1 = test::MakeFormGlobalId();
  FormGlobalId form2 = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form1}, base::span<FormGlobalId>());
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form2}, base::span<FormGlobalId>());

  base::test::TestFuture<void> future;
  tracker().Wait(future.GetCallback(), base::Milliseconds(1000));

  // Fully parse form 1.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAfterFormsSeen,
      std::vector<FormGlobalId>{form1}, base::span<FormGlobalId>());
  EXPECT_FALSE(future.IsReady());

  // Fully parse form 2, callback should be executed.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAfterFormsSeen,
      std::vector<FormGlobalId>{form2}, base::span<FormGlobalId>());
  EXPECT_TRUE(future.Wait());
}

// Tests that calling Wait() while a callback is already registered, schedules
// another callback, all of which will be executed once requirements are met.
TEST_F(FormPredictionsTrackerTest, Wait_MultipleCallbacksPending) {
  FormGlobalId form_id = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form_id}, base::span<FormGlobalId>());
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeLoadedServerPredictions,
      std::vector<FormGlobalId>{form_id});

  base::test::TestFuture<void> future1;
  base::test::TestFuture<void> future2;

  tracker().Wait(future1.GetCallback(), base::Milliseconds(1000));
  tracker().Wait(future2.GetCallback(), base::Milliseconds(1000));
  EXPECT_EQ(2UL, test_api(tracker()).num_callbacks());

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAfterFormsSeen,
      std::vector<FormGlobalId>{form_id}, base::span<FormGlobalId>());
  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  // Both callbacks should be executed when requirements are met.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAfterLoadedServerPredictions,
      std::vector<FormGlobalId>{form_id});

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
      &AutofillManager::Observer::OnAfterFormsSeen,
      std::vector<FormGlobalId>{form1}, base::span<FormGlobalId>());
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

  // Form 2 gets fully parsed, future2 should be ready.
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnAfterFormsSeen,
      std::vector<FormGlobalId>{form2}, base::span<FormGlobalId>());
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
      &AutofillManager::Observer::OnAfterFormsSeen,
      std::vector<FormGlobalId>{form}, base::span<FormGlobalId>());
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
      &AutofillManager::Observer::OnAfterFormsSeen,
      std::vector<FormGlobalId>{form}, base::span<FormGlobalId>());
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

// Verifies that if the tab is not in active actor mode, there is no waiting.
TEST_F(FormPredictionsTrackerTest, Wait_NoActiveActor) {
  autofill_client().set_is_tab_in_actor_mode(false);

  FormGlobalId form_id = test::MakeFormGlobalId();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeFormsSeen,
      std::vector<FormGlobalId>{form_id}, base::span<FormGlobalId>());

  // Since the tab is not in actor mode, there should be no waiting.
  base::test::TestFuture<void> future;
  tracker().Wait(future.GetCallback(), base::Milliseconds(1000));
  EXPECT_TRUE(future.Wait());
}

// Verifies that calling AutofillManager::ReparseKnownForms() triggers form
// parsing observer events.
//
// Note: In unit tests, `autofill_manager()` is a `TestBrowserAutofillManager`,
// which overrides `OnFormsSeen()` to block until asynchronous parsing finishes
// and `OnAfterFormsSeen()` has fired. Therefore, `forms_in_parsing_state_` is
// empty immediately after `OnFormsSeen()` (and `ReparseKnownForms()`) returns.
// We use a `MockAutofillManagerObserver` to verify that `ReparseKnownForms()`
// fires both `OnBeforeFormsSeen()` and `OnAfterFormsSeen()` during its
// execution.
TEST_F(FormPredictionsTrackerTest, Wait_ReparseKnownForms) {
  FormData form = test::CreateTestAddressFormData();
  autofill_manager().OnFormsSeen({form}, {},
                                 AutofillManagerTestApi::pass_key());
  ASSERT_TRUE(test_api(tracker()).forms_in_parsing_state().empty());

  MockAutofillManagerObserver observer;
  autofill_manager().AddObserver(&observer);
  EXPECT_CALL(
      observer,
      OnBeforeFormsSeen(::testing::_, ::testing::ElementsAre(form.global_id()),
                        ::testing::IsEmpty()));
  EXPECT_CALL(
      observer,
      OnAfterFormsSeen(::testing::_, ::testing::ElementsAre(form.global_id()),
                       ::testing::IsEmpty()));
  autofill_manager().ReparseKnownForms();
  autofill_manager().RemoveObserver(&observer);
}

}  // namespace

}  // namespace autofill
