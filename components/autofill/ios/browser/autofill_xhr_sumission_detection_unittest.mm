// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <optional>
#import <set>

#import "base/ranges/algorithm.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "components/autofill/core/browser/test_autofill_client.h"
#import "components/autofill/core/common/field_data_manager.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/form_field_data.h"
#import "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/test_autofill_manager_injector.h"
#import "components/autofill/ios/common/field_data_manager_factory_ios.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using ::testing::ElementsAre;
using ::testing::Property;

namespace autofill {

class TestingAutofillManager : public BrowserAutofillManager {
 public:
  explicit TestingAutofillManager(AutofillDriverIOS* driver)
      : BrowserAutofillManager(driver, "en-US") {}

  void OnFormSubmitted(const FormData& form,
                       const bool known_success,
                       const mojom::SubmissionSource source) override {
    submitted_form_ = form;
    BrowserAutofillManager::OnFormSubmitted(form, known_success, source);
  }

  const std::optional<FormData>& submitted_form() const {
    return submitted_form_;
  }

 private:
  std::optional<FormData> submitted_form_ = std::nullopt;
};

// Test fixture for validating async form submission detection logic in
// AutofillDriverIOS.
class AutofillXHRSubmissionDetectionTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    histogram_tester_ = std::make_unique<base::HistogramTester>();

    // Setup fake frames injection in the content world used by Autofill
    // features.
    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = web_frames_manager.get();
    web::ContentWorld content_world =
        AutofillJavaScriptFeature::GetInstance()->GetSupportedContentWorld();
    web_state_.SetWebFramesManager(content_world,
                                   std::move(web_frames_manager));

    // Driver factory needs to exist before any call to
    // `AutofillDriverIOS::FromWebStateAndWebFrame`, or we crash.
    autofill::AutofillDriverIOSFactory::CreateForWebState(
        &web_state_, &autofill_client_, /*bridge=*/nil,
        /*locale=*/"en");

    // Replace AutofillManager with the test implementation.
    autofill_manager_injector_ =
        std::make_unique<TestAutofillManagerInjector<TestingAutofillManager>>(
            &web_state_);

    // Inject a fake main frame.
    auto main_frame =
        web::FakeWebFrame::CreateMainWebFrame(GURL("https://example.com"));

    web_frames_manager_->AddWebFrame(std::move(main_frame));
  }

  web::WebFrame* main_frame() { return web_frames_manager_->GetMainWebFrame(); }

  AutofillDriverIOS* main_frame_driver() {
    return AutofillDriverIOS::FromWebStateAndWebFrame(&web_state_,
                                                      main_frame());
  }

  TestingAutofillManager& main_frame_manager() {
    return static_cast<TestingAutofillManager&>(
        main_frame_driver()->GetAutofillManager());
  }

  base::test::TaskEnvironment task_environment_;
  autofill::TestAutofillClient autofill_client_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeWebFramesManager> web_frames_manager_;
  std::unique_ptr<TestAutofillManagerInjector<TestingAutofillManager>>
      autofill_manager_injector_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests that typing values in forms and removing them triggers a submission
// detection.
TEST_F(AutofillXHRSubmissionDetectionTest,
       SubmissionDetectedAfterLastInteractedFormRemoved) {
  // Create two dummy FormData to simulate interaction and removal.
  FormData form_data1;
  form_data1.set_renderer_id(FormRendererId(1));
  FormFieldData form_field_data1;
  form_field_data1.set_renderer_id(FieldRendererId(2));
  form_field_data1.set_host_form_id(form_data1.renderer_id());
  form_data1.set_fields({form_field_data1});

  FormData form_data2;
  form_data2.set_renderer_id(FormRendererId(3));
  FormFieldData form_field_data2;
  form_field_data2.set_renderer_id(FieldRendererId(4));
  form_field_data2.set_host_form_id(form_data2.renderer_id());
  FormFieldData form_field_data3;
  form_field_data3.set_renderer_id(FieldRendererId(5));
  form_field_data3.set_host_form_id(form_data2.renderer_id());

  // Simulate typing in the first form.
  auto* autofill_driver = main_frame_driver();
  ASSERT_TRUE(autofill_driver);
  autofill_driver->TextFieldDidChange(form_data1, form_field_data1.global_id(),
                                      base::TimeTicks::Now());
  // Simulate typing in the first field of the second form.
  form_field_data2.set_value(u"value2");
  form_data2.set_fields({form_field_data2, form_field_data3});
  autofill_driver->TextFieldDidChange(form_data2, form_field_data2.global_id(),
                                      base::TimeTicks::Now());

  // Simulate typing on the other field of the second form.
  form_field_data3.set_value(u"value3");
  form_data2.set_fields({form_field_data2, form_field_data3});
  autofill_driver->TextFieldDidChange(form_data2, form_field_data3.global_id(),
                                      base::TimeTicks::Now());
  // Simulate forms removal.
  autofill_driver->FormsRemoved(
      /*removed_forms=*/{form_data1.renderer_id(), form_data2.renderer_id()},
      /*removed_unowned_fields=*/{});

  // Validate that last interacted form was detected as submitted and sent to
  // AutofillManager.
  auto& autofill_manager = main_frame_manager();
  ASSERT_TRUE(autofill_manager.submitted_form());
  // Check that the submitted form has the values "typed" in each field.
  EXPECT_TRUE(
      FormData::DeepEqual(*autofill_manager.submitted_form(), form_data2));
  EXPECT_THAT(autofill_manager.submitted_form()->fields(),
              ElementsAre(Property(&FormFieldData::value, u"value2"),
                          Property(&FormFieldData::value, u"value3")));

  histogram_tester_->ExpectUniqueSample(
      /*name=*/kAutofillSubmissionDetectionSourceHistogram,
      /*sample=*/mojom::SubmissionSource::XHR_SUCCEEDED,
      /*expected_bucket_count=*/1);
  histogram_tester_->ExpectUniqueSample(
      /*name=*/kFormSubmissionAfterFormRemovalHistogram, /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester_->ExpectUniqueSample(
      /*name=*/kFormRemovalRemovedUnownedFieldsHistogram, /*sample=*/0,
      /*expected_bucket_count=*/1);
}

// Tests that autofilling a form and removing it triggers a submission
// detection.
TEST_F(AutofillXHRSubmissionDetectionTest,
       SubmissionDetectedAfterLastAutofilledFormRemoved) {
  // Create a dummy FormData to simulate interaction and removal.
  FormData form_data;
  form_data.set_renderer_id(FormRendererId(1));
  FormFieldData form_field_data;
  form_field_data.set_renderer_id(FieldRendererId(2));
  form_field_data.set_host_form_id(form_data.renderer_id());
  form_field_data.set_value(u"value");
  form_data.set_fields({form_field_data});

  // Simulate autofilling the form.
  auto* autofill_driver = main_frame_driver();
  ASSERT_TRUE(autofill_driver);
  autofill_driver->DidFillAutofillFormData(form_data, base::TimeTicks::Now());

  // Simulate form removal.
  autofill_driver->FormsRemoved(/*removed_forms=*/{form_data.renderer_id()},
                                /*removed_unowned_fields=*/{});

  // Validate that the form was detected as submitted and sent to
  // AutofillManager.
  auto& autofill_manager = main_frame_manager();
  ASSERT_TRUE(autofill_manager.submitted_form());
  EXPECT_TRUE(
      FormData::DeepEqual(*autofill_manager.submitted_form(), form_data));
  EXPECT_THAT(autofill_manager.submitted_form()->fields(),
              ElementsAre(Property(&FormFieldData::value, u"value")));

  histogram_tester_->ExpectUniqueSample(
      /*name=*/kAutofillSubmissionDetectionSourceHistogram,
      /*sample=*/mojom::SubmissionSource::XHR_SUCCEEDED,
      /*expected_count=*/1);
  histogram_tester_->ExpectUniqueSample(
      /*name=*/kFormSubmissionAfterFormRemovalHistogram, /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester_->ExpectUniqueSample(
      /*name=*/kFormRemovalRemovedUnownedFieldsHistogram, /*sample=*/0,
      /*expected_bucket_count=*/1);
}

// Tests that typing values in formless fields and then removing the last
// interacted one triggers a submission detection.
TEST_F(AutofillXHRSubmissionDetectionTest,
       SubmissionDetectedAfterFormlessFieldsRemoved) {
  // Create a dummy formless FormData to simulate interaction and removal.
  FormData form_data;
  // Explicitly setting "formless form" renderer id for clarity.
  form_data.set_renderer_id(FormRendererId(0));
  // Create two fields.
  FormFieldData form_field_data1;
  form_field_data1.set_renderer_id(FieldRendererId(1));
  form_field_data1.set_host_form_id(form_data.renderer_id());
  FormFieldData form_field_data2;
  form_field_data2.set_renderer_id(FieldRendererId(2));
  form_field_data2.set_host_form_id(form_data.renderer_id());
  form_data.set_fields({form_field_data1, form_field_data2});

  // Simulate the user updating the first field.
  auto* autofill_driver = main_frame_driver();
  ASSERT_TRUE(autofill_driver);
  form_field_data1.set_value(u"value1");
  form_data.set_fields({form_field_data1, form_field_data2});
  autofill_driver->TextFieldDidChange(form_data, form_field_data1.global_id(),
                                      base::TimeTicks::Now());

  // Simulate the user updating the second field.
  form_field_data2.set_value(u"value2");
  form_data.set_fields({form_field_data1, form_field_data2});
  autofill_driver->TextFieldDidChange(form_data, form_field_data2.global_id(),
                                      base::TimeTicks::Now());

  // Simulate the removal of the first formless field.
  autofill_driver->FormsRemoved(
      /*removed_forms=*/{},
      /*removed_unowned_fields=*/{form_field_data1.renderer_id()});

  // No submission detected because the first field did not receive the last
  // user interaction.
  auto& autofill_manager = main_frame_manager();
  EXPECT_FALSE(autofill_manager.submitted_form());

  histogram_tester_->ExpectTotalCount(
      /*name=*/kAutofillSubmissionDetectionSourceHistogram,
      /*expected_count=*/0);
  histogram_tester_->ExpectUniqueSample(
      /*name=*/kFormSubmissionAfterFormRemovalHistogram, /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester_->ExpectUniqueSample(
      /*name=*/kFormRemovalRemovedUnownedFieldsHistogram, /*sample=*/1,
      /*expected_bucket_count=*/1);

  // Reset histogram stats and measure second removal event.
  histogram_tester_ = std::make_unique<base::HistogramTester>();

  // Simulate the removal of the second field.
  autofill_driver->FormsRemoved(
      /*removed_forms=*/{},
      /*removed_unowned_fields=*/{form_field_data2.renderer_id()});

  // Validate that the formless form was detected as submitted and sent to
  // AutofillManager.
  ASSERT_TRUE(autofill_manager.submitted_form());
  EXPECT_TRUE(
      FormData::DeepEqual(*autofill_manager.submitted_form(), form_data));
  EXPECT_THAT(autofill_manager.submitted_form()->fields(),
              ElementsAre(Property(&FormFieldData::value, u"value1"),
                          Property(&FormFieldData::value, u"value2")));

  histogram_tester_->ExpectUniqueSample(
      /*name=*/kAutofillSubmissionDetectionSourceHistogram,
      /*sample=*/mojom::SubmissionSource::XHR_SUCCEEDED,
      /*expected_count=*/1);
  histogram_tester_->ExpectUniqueSample(
      /*name=*/kFormSubmissionAfterFormRemovalHistogram, /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester_->ExpectUniqueSample(
      /*name=*/kFormRemovalRemovedUnownedFieldsHistogram, /*sample=*/1,
      /*expected_bucket_count=*/1);
}

// Tests that no submission is detected if a form is removed without user
// interactions with it.
TEST_F(AutofillXHRSubmissionDetectionTest,
       NoSubmissionDetectedAfterFormRemovedWithoutInteractions) {
  // Create a dummy FormData to simulate removal.
  FormData form_data;
  form_data.set_renderer_id(FormRendererId(1));
  FormFieldData form_field_data;
  form_field_data.set_renderer_id(FieldRendererId(2));
  form_field_data.set_host_form_id(form_data.renderer_id());
  form_data.set_fields({form_field_data});

  auto* autofill_driver = main_frame_driver();
  ASSERT_TRUE(autofill_driver);
  // Simulate form removal without interactions.
  autofill_driver->FormsRemoved(/*removed_forms=*/{form_data.renderer_id()},
                                /*removed_unowned_fields=*/{});

  // Validate that no form was sent to AutfillManager as submitted.
  auto& autofill_manager = main_frame_manager();
  EXPECT_FALSE(autofill_manager.submitted_form());

  histogram_tester_->ExpectTotalCount(
      /*name=*/kAutofillSubmissionDetectionSourceHistogram,
      /*expected_count=*/0);
  histogram_tester_->ExpectUniqueSample(
      /*name=*/kFormSubmissionAfterFormRemovalHistogram, /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester_->ExpectUniqueSample(
      /*name=*/kFormRemovalRemovedUnownedFieldsHistogram, /*sample=*/0,
      /*expected_bucket_count=*/1);
}

// Tests that a removed form detected as submitted is updated with data from
// FieldDataManager.
TEST_F(AutofillXHRSubmissionDetectionTest,
       SubmittedFormUpdatedFromFieldDataManager) {
  // Create a dummy FormData to simulate interaction and removal.
  FormData form_data;
  form_data.set_renderer_id(FormRendererId(1));
  FormFieldData form_field_data;
  form_field_data.set_renderer_id(FieldRendererId(2));
  form_field_data.set_host_form_id(form_data.renderer_id());
  form_field_data.set_value(u"value1");
  form_data.set_fields({form_field_data});

  // Simulate the user updating the form field.
  auto* autofill_driver = main_frame_driver();
  ASSERT_TRUE(autofill_driver);
  autofill_driver->TextFieldDidChange(form_data, form_field_data.global_id(),
                                      base::TimeTicks::Now());

  // Update the form field in FieldDataManager.
  std::u16string data_manager_value = u"value2";
  auto data_manager_mask = FieldPropertiesFlags::kUserTyped;
  FieldDataManager* fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(main_frame());
  fieldDataManager->UpdateFieldDataMap(form_field_data.renderer_id(),
                                       data_manager_value, data_manager_mask);

  // Simulate form removal.
  autofill_driver->FormsRemoved(/*removed_forms=*/{form_data.renderer_id()},
                                /*removed_unowned_fields=*/{});

  // Validate that form was detected as submitted and sent to
  // AutofillManager with the field data updated with the value in
  // FieldDataManager.
  auto& autofill_manager = main_frame_manager();
  ASSERT_TRUE(autofill_manager.submitted_form());
  EXPECT_TRUE(
      FormData::DeepEqual(form_data, *autofill_manager.submitted_form()));
  EXPECT_THAT(autofill_manager.submitted_form()->fields(),
              ElementsAre(Property(&FormFieldData::value, u"value2")));
}

}  // namespace autofill
