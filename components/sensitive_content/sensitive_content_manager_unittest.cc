// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sensitive_content/sensitive_content_manager.h"

#include "base/base64.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/content/browser/autofill_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_test_api.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_manager_test_api.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/sensitive_content/features.h"
#include "components/sensitive_content/sensitive_content_client.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace sensitive_content {
namespace {

using autofill::AutofillManager;
using autofill::AutofillManagerEvent;
using autofill::AutofillQueryResponse;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::TestAutofillManagerWaiter;
using ::testing::InSequence;
using ::testing::MockFunction;
using LifecycleState = autofill::AutofillDriver::LifecycleState;

constexpr std::string_view histogram_sensitive_time =
    "SensitiveContent.Chrome.SensitiveTime";
constexpr std::string_view histogram_sensitivity_changed =
    "SensitiveContent.Chrome.SensitivityChanged";
constexpr std::string_view histogram_latency_until_sensitive =
    "SensitiveContent.Chrome.LatencyUntilSensitive";

std::optional<std::string> CreateSensitiveServerPredictions(
    const FormData& form) {
  AutofillQueryResponse response;
  AutofillQueryResponse::FormSuggestion* form_suggestion;
  std::string response_string;

  form_suggestion = response.add_form_suggestions();
  autofill::test::AddFieldPredictionToForm(
      form.fields()[0], autofill::FieldType::ACCOUNT_CREATION_PASSWORD,
      form_suggestion);
  if (!response.SerializeToString(&response_string)) {
    return std::nullopt;
  }
  return response_string;
}

class MockSensitiveContentClient : public SensitiveContentClient {
 public:
  MOCK_METHOD(void, SetContentSensitivity, (bool), (override));

  std::string_view GetHistogramPrefix() override {
    return "SensitiveContent.Chrome.";
  }
};

class SensitiveContentManagerTest : public content::RenderViewHostTestHarness {
 public:
  SensitiveContentManagerTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    sensitive_content_manager_ = std::make_unique<SensitiveContentManager>(
        web_contents(), &sensitive_content_client_);
  }

  void TearDown() override {
    // The destruction of the frame at the end of a test triggers a state
    // change, which can result in content sensitivity being changed and in
    // unexpected calls to the client.
    testing::Mock::VerifyAndClearExpectations(&sensitive_content_client_);
    content::RenderViewHostTestHarness::TearDown();
  }

  autofill::ContentAutofillDriver* autofill_driver() {
    return autofill_driver_injector_[web_contents()];
  }

  AutofillManager& autofill_manager() {
    return autofill_driver()->GetAutofillManager();
  }

  MockSensitiveContentClient& sensitive_content_client() {
    return sensitive_content_client_;
  }

  SensitiveContentManager& sensitive_content_manager() {
    return *sensitive_content_manager_;
  }

  // Creates a `FormData` that is not sensitive and sets its `LocalFrameToken`
  // to the one of the `autofill_driver()`, to mimic production behavior. The
  // proper functioning of
  // `SensitiveContentManager::OnAutofillManagerStateChanged()` depends on
  // `LocalFrameToken`s being set properly.
  FormData CreateNotSensitiveFormData() {
    return autofill::test::CreateFormDataForFrame(
        autofill::test::CreateTestAddressFormData(),
        autofill_driver()->GetFrameToken());
  }

  // Creates a `FormData` that is sensitive and sets its `LocalFrameToken` to
  // the one of the `autofill_driver()`, to mimic production behavior. The
  // proper functioning of
  // `SensitiveContentManager::OnAutofillManagerStateChanged()` depends on
  // `LocalFrameToken`s being set properly.
  FormData CreateSensitiveFormData() {
    return autofill::test::CreateFormDataForFrame(
        autofill::test::CreateTestCreditCardFormData(/*is_https=*/false,
                                                     /*use_month_type=*/false),
        autofill_driver()->GetFrameToken());
  }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  // Needed to get the driver factory initialized.
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      autofill_client_injector_;
  autofill::TestAutofillDriverInjector<autofill::ContentAutofillDriver>
      autofill_driver_injector_;
  MockSensitiveContentClient sensitive_content_client_;
  std::unique_ptr<SensitiveContentManager> sensitive_content_manager_;
};

TEST_F(SensitiveContentManagerTest, AddAndRemoveSensitiveAndNotSensitiveForms) {
  NavigateAndCommit(GURL("https://test.com"));
  FormData not_sensitive_form = CreateNotSensitiveFormData();
  FormData sensitive_form = CreateSensitiveFormData();
  base::HistogramTester histogram_tester;

  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(sensitive_content_client(), SetContentSensitivity).Times(0);
    EXPECT_CALL(check, Call("no sensitive content present"));
    EXPECT_CALL(sensitive_content_client(),
                SetContentSensitivity(/*content_is_sensitive=*/true));
    EXPECT_CALL(check, Call("sensitive content present"));
    EXPECT_CALL(sensitive_content_client(),
                SetContentSensitivity(/*content_is_sensitive=*/false));
    EXPECT_CALL(check, Call("no sensitive content present anymore"));
    EXPECT_CALL(sensitive_content_client(), SetContentSensitivity).Times(0);
  }

  TestAutofillManagerWaiter waiter(autofill_manager(),
                                   {AutofillManagerEvent::kFormsSeen});
  autofill_manager().OnFormsSeen(/*updated_forms=*/{not_sensitive_form},
                                 /*removed_forms=*/{});
  ASSERT_TRUE(waiter.Wait());
  check.Call("no sensitive content present");
  histogram_tester.ExpectTotalCount(histogram_sensitivity_changed, 0);

  autofill_manager().OnFormsSeen(/*updated_forms=*/{sensitive_form},
                                 /*removed_forms=*/{});
  ASSERT_TRUE(waiter.Wait());
  check.Call("sensitive content present");
  histogram_tester.ExpectUniqueSample(histogram_sensitivity_changed,
                                      /*content_is_sensitive=*/true, 1);

  autofill_manager().OnFormsSeen(
      /*updated_forms=*/{},
      /*removed_forms=*/{sensitive_form.global_id()});
  ASSERT_TRUE(waiter.Wait());
  check.Call("no sensitive content present anymore");
  histogram_tester.ExpectBucketCount(histogram_sensitivity_changed,
                                     /*content_is_sensitive=*/true, 1);
  histogram_tester.ExpectBucketCount(histogram_sensitivity_changed,
                                     /*content_is_sensitive=*/false, 1);

  autofill_manager().OnFormsSeen(
      /*updated_forms=*/{},
      /*removed_forms=*/{not_sensitive_form.global_id()});
  ASSERT_TRUE(waiter.Wait());
}

TEST_F(SensitiveContentManagerTest, AutofillManagerStateChanged) {
  NavigateAndCommit(GURL("https://test.com"));
  FormData not_sensitive_form = CreateNotSensitiveFormData();
  FormData sensitive_form = CreateSensitiveFormData();
  base::HistogramTester histogram_tester;

  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(sensitive_content_client(), SetContentSensitivity).Times(0);
    EXPECT_CALL(check, Call("no sensitive content present so far"));
    EXPECT_CALL(sensitive_content_client(),
                SetContentSensitivity(/*content_is_sensitive=*/true));
    EXPECT_CALL(check, Call("sensitive content present now"));
    EXPECT_CALL(sensitive_content_client(),
                SetContentSensitivity(/*content_is_sensitive=*/false));
    EXPECT_CALL(check, Call("frame became inactive"));
    EXPECT_CALL(sensitive_content_client(),
                SetContentSensitivity(/*content_is_sensitive=*/true));
    EXPECT_CALL(check, Call("frame became active"));
  }

  test_api(*autofill_driver())
      .SetLifecycleStateAndNotifyObservers(LifecycleState::kActive);

  TestAutofillManagerWaiter waiter(autofill_manager(),
                                   {AutofillManagerEvent::kFormsSeen});
  autofill_manager().OnFormsSeen(/*updated_forms=*/{not_sensitive_form},
                                 /*removed_forms=*/{});
  ASSERT_TRUE(waiter.Wait());

  test_api(*autofill_driver())
      .SetLifecycleStateAndNotifyObservers(LifecycleState::kInactive);
  test_api(*autofill_driver())
      .SetLifecycleStateAndNotifyObservers(LifecycleState::kActive);
  check.Call("no sensitive content present so far");
  histogram_tester.ExpectTotalCount(histogram_sensitivity_changed, 0);

  autofill_manager().OnFormsSeen(/*updated_forms=*/{sensitive_form},
                                 /*removed_forms=*/{});
  ASSERT_TRUE(waiter.Wait());
  check.Call("sensitive content present now");
  histogram_tester.ExpectUniqueSample(histogram_sensitivity_changed,
                                      /*content_is_sensitive=*/true, 1);

  test_api(*autofill_driver())
      .SetLifecycleStateAndNotifyObservers(LifecycleState::kInactive);
  check.Call("frame became inactive");
  histogram_tester.ExpectBucketCount(histogram_sensitivity_changed,
                                     /*content_is_sensitive=*/true, 1);
  histogram_tester.ExpectBucketCount(histogram_sensitivity_changed,
                                     /*content_is_sensitive=*/false, 1);

  test_api(*autofill_driver())
      .SetLifecycleStateAndNotifyObservers(LifecycleState::kActive);
  check.Call("frame became active");
  histogram_tester.ExpectBucketCount(histogram_sensitivity_changed,
                                     /*content_is_sensitive=*/true, 2);
  histogram_tester.ExpectBucketCount(histogram_sensitivity_changed,
                                     /*content_is_sensitive=*/false, 1);
}

TEST_F(SensitiveContentManagerTest, LatencyUntilSensitiveMetricRecorded) {
  NavigateAndCommit(GURL("https://test.com"));
  // The form is not considered sensitive by heuristics. The form will be
  // considered sensitive only after server predictions are served.
  FormData server_predictions_sensitive_form = CreateNotSensitiveFormData();
  base::HistogramTester histogram_tester;

  // Simulate the form being detected in the DOM. The heuristics will consider
  // the form as not sensitive.
  autofill::TestAutofillManagerSingleEventWaiter wait_for_forms_seen(
      autofill_manager(), &AutofillManager::Observer::OnAfterFormsSeen);
  autofill_manager().OnFormsSeen(
      /*updated_forms=*/{server_predictions_sensitive_form},
      /*removed_forms=*/{});
  ASSERT_TRUE(std::move(wait_for_forms_seen).Wait());

  // Mock a delay between the start of parsing and receiving the server
  // predictions.
  task_environment()->FastForwardBy(base::Milliseconds(100));

  EXPECT_CALL(sensitive_content_client(),
              SetContentSensitivity(/*content_is_sensitive=*/true));
  // Simulate sensitive server predictions for the form.
  autofill::FormStructure form_structure(server_predictions_sensitive_form);
  std::optional<std::string> response_string =
      CreateSensitiveServerPredictions(server_predictions_sensitive_form);
  ASSERT_TRUE(response_string.has_value());
  test_api(autofill_manager())
      .OnLoadedServerPredictions(
          base::Base64Encode(response_string.value()),
          autofill::test::GetEncodedSignatures(form_structure));

  histogram_tester.ExpectUniqueTimeSample(histogram_latency_until_sensitive,
                                          base::Milliseconds(100), 1);
}

TEST_F(SensitiveContentManagerTest, SensitiveTimeMetricRecorded) {
  NavigateAndCommit(GURL("https://test.com"));
  FormData sensitive_form = CreateSensitiveFormData();
  base::HistogramTester histogram_tester;

  TestAutofillManagerWaiter waiter(autofill_manager(),
                                   {AutofillManagerEvent::kFormsSeen});
  autofill_manager().OnFormsSeen(
      /*updated_forms=*/{sensitive_form},
      /*removed_forms=*/{});
  ASSERT_TRUE(waiter.Wait());

  task_environment()->FastForwardBy(base::Milliseconds(100));

  autofill_manager().OnFormsSeen(
      /*updated_forms=*/{},
      /*removed_forms=*/{sensitive_form.global_id()});
  ASSERT_TRUE(waiter.Wait());

  histogram_tester.ExpectUniqueTimeSample(histogram_sensitive_time,
                                          base::Milliseconds(100), 1);
}

class SensitiveContentManagerPwmHeuristicsTest
    : public SensitiveContentManagerTest,
      public testing::WithParamInterface<bool> {
 public:
  bool UsePwmHeuristics() const { return GetParam(); }
};

TEST_P(SensitiveContentManagerPwmHeuristicsTest, UsePwmHeuristics) {
  base::test::ScopedFeatureList scoped_feature_list;
  FormData form = autofill::test::CreateFormDataForRenderFrameHost(
      *main_rfh(),
      {autofill::test::CreateTestFormField(
           "Username", "username", "", autofill::FormControlType::kInputText),
       autofill::test::CreateTestFormField(
           "Password", "password", "",
           autofill::FormControlType::kInputPassword)});
  NavigateAndCommit(GURL("https://test.com"));

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kSensitiveContent,
      {{features::kSensitiveContentUsePwmHeuristicsParam.name,
        UsePwmHeuristics() ? "true" : "false"}});

  if (UsePwmHeuristics()) {
    // With the feature param enabled, the form will be reparsed by password
    // manager, and considered sensitive.
    EXPECT_CALL(sensitive_content_client(),
                SetContentSensitivity(/*content_is_sensitive=*/true));
  } else {
    // With the feature param disabled, the form will not be reparsed by
    // password manager, and not considered sensitive.
    EXPECT_CALL(sensitive_content_client(), SetContentSensitivity).Times(0);
  }
  TestAutofillManagerWaiter waiter(autofill_manager(),
                                   {AutofillManagerEvent::kFormsSeen});
  autofill_driver()->renderer_events().FormsSeen(/*updated_forms=*/{form},
                                                 /*removed_forms=*/{});
  ASSERT_TRUE(waiter.Wait(/*num_expected_relevant_events=*/1));
}

INSTANTIATE_TEST_SUITE_P(SensitiveContentManagerTest,
                         SensitiveContentManagerPwmHeuristicsTest,
                         ::testing::Bool());

}  // namespace
}  // namespace sensitive_content
