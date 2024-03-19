// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/form_activity_tab_helper.h"
#include "base/test/metrics/histogram_tester.h"

#import "base/test/ios/wait_util.h"
#import "components/autofill/ios/form_util/autofill_test_with_web_state.h"
#import "components/autofill/ios/form_util/form_activity_observer.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "components/autofill/ios/form_util/test_form_activity_observer.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_state_observer_util.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/platform_test.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using web::WebFrame;

// Tests fixture for autofill::FormActivityTabHelper class.
class FormActivityTabHelperTest : public AutofillTestWithWebState,
                                  public testing::WithParamInterface<bool> {
 public:
  FormActivityTabHelperTest()
      : AutofillTestWithWebState(std::make_unique<web::FakeWebClient>()) {
    web::FakeWebClient* web_client =
        static_cast<web::FakeWebClient*>(GetWebClient());
    web_client->SetJavaScriptFeatures(
        {autofill::FormUtilJavaScriptFeature::GetInstance(),
         autofill::FormHandlersJavaScriptFeature::GetInstance()});
  }

  void SetUp() override {
    web::WebTestWithWebState::SetUp();

    autofill::FormActivityTabHelper* tab_helper =
        autofill::FormActivityTabHelper::GetOrCreateForWebState(web_state());
    observer_ =
        std::make_unique<autofill::TestFormActivityObserver>(web_state());
    tab_helper->AddObserver(observer_.get());
  }

  void TearDown() override {
    autofill::FormActivityTabHelper* tab_helper =
        autofill::FormActivityTabHelper::GetOrCreateForWebState(web_state());
    tab_helper->RemoveObserver(observer_.get());
    web::WebTestWithWebState::TearDown();
  }

 protected:
  WebFrame* WaitForMainFrame() {
    __block WebFrame* main_frame = nullptr;
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
      web::WebFramesManager* frames_manager =
          autofill::FormUtilJavaScriptFeature::GetInstance()
              ->GetWebFramesManager(web_state());
      main_frame = frames_manager->GetMainWebFrame();
      return main_frame != nullptr;
    }));
    return main_frame;
  }

  base::HistogramTester histogram_tester_;
  std::unique_ptr<autofill::TestFormActivityObserver> observer_;
};

// Tests that observer is called on form submission using submit control.
TEST_P(FormActivityTabHelperTest, TestObserverDocumentSubmitted) {
  LoadHtml(@"<form name='form-name'>"
            "<input type='submit' id='submit'/>"
            "</form>");

  WebFrame* main_frame = WaitForMainFrame();
  ASSERT_TRUE(main_frame);
  SetUpForUniqueIds(main_frame);

  ASSERT_FALSE(observer_->submit_document_info());
  const std::string kTestFormName("form-name");

  std::string mainFrameID = main_frame->GetFrameId();
  const std::string kTestFormData =
      std::string("[{\"name\":\"form-name\",\"origin\":\"https://chromium.test/"
                  "\",\"action\":\"https://chromium.test/\","
                  "\"name_attribute\":\"form-name\",\"id_attribute\":\"\","
                  "\"renderer_id\":\"1\",\"frame_id\":\"") +
      mainFrameID + std::string("\"}]");

  ExecuteJavaScript(@"document.getElementById('submit').click();");
  ASSERT_TRUE(observer_->submit_document_info());
  EXPECT_EQ(web_state(), observer_->submit_document_info()->web_state);
  EXPECT_EQ(main_frame, observer_->submit_document_info()->sender_frame);
  EXPECT_EQ(kTestFormName, observer_->submit_document_info()->form_name);
  EXPECT_EQ(kTestFormData, observer_->submit_document_info()->form_data);

  EXPECT_FALSE(observer_->submit_document_info()->has_user_gesture);

  // Verify that there isn't any form activity metric recorded, even if
  // batching is allowed, as the form submit signals aren't covered.
  histogram_tester_.ExpectTotalCount("Autofill.iOS.FormActivity.DropCount", 0);
  histogram_tester_.ExpectTotalCount("Autofill.iOS.FormActivity.SendCount", 0);
  histogram_tester_.ExpectTotalCount("Autofill.iOS.FormActivity.SendRatio", 0);
}

// Tests that observer is called on form submission using submit() method.
TEST_P(FormActivityTabHelperTest, TestFormSubmittedHook) {
  LoadHtml(@"<form name='form-name' id='form'>"
            "<input type='submit'/>"
            "</form>");

  WebFrame* main_frame = WaitForMainFrame();
  ASSERT_TRUE(main_frame);
  SetUpForUniqueIds(main_frame);

  ASSERT_FALSE(observer_->submit_document_info());
  const std::string kTestFormName("form-name");

  std::string mainFrameID = main_frame->GetFrameId();
  const std::string kTestFormData =
      std::string("[{\"name\":\"form-name\",\"origin\":\"https://chromium.test/"
                  "\",\"action\":\"https://chromium.test/\","
                  "\"name_attribute\":\"form-name\",\"id_attribute\":\"form\","
                  "\"renderer_id\":\"1\",\"frame_id\":\"") +
      mainFrameID + std::string("\"}]");

  ExecuteJavaScript(@"document.getElementById('form').submit();");
  ASSERT_TRUE(observer_->submit_document_info());
  EXPECT_EQ(web_state(), observer_->submit_document_info()->web_state);
  EXPECT_EQ(main_frame, observer_->submit_document_info()->sender_frame);
  EXPECT_EQ(kTestFormName, observer_->submit_document_info()->form_name);
  EXPECT_EQ(kTestFormData, observer_->submit_document_info()->form_data);
  EXPECT_FALSE(observer_->submit_document_info()->has_user_gesture);

  // Verify that there isn't any form activity metric recorded, even if
  // batching is allowed, as the form submit signals aren't covered.
  histogram_tester_.ExpectTotalCount("Autofill.iOS.FormActivity.DropCount", 0);
  histogram_tester_.ExpectTotalCount("Autofill.iOS.FormActivity.SendCount", 0);
  histogram_tester_.ExpectTotalCount("Autofill.iOS.FormActivity.SendRatio", 0);
}

// Tests that observer is called on form activity (input event).
// TODO(crbug.com/1431960): Disabled test due to bot failure. Re-enable when
// fixed.
TEST_P(FormActivityTabHelperTest,
       DISABLED_TestObserverFormActivityFrameMessaging) {
  LoadHtml(@"<form name='form-name'>"
            "<input type='input' name='field-name' id='fieldid'/>"
            "</form>");

  WebFrame* main_frame = WaitForMainFrame();
  ASSERT_TRUE(main_frame);
  SetUpForUniqueIds(main_frame);

  ASSERT_FALSE(observer_->form_activity_info());
  // First call will set document.activeElement (which is usually set by user
  // action. Second call will trigger the message.
  ExecuteJavaScript(@"document.getElementById('fieldid').focus();");
  ASSERT_FALSE(observer_->form_activity_info());
  ExecuteJavaScript(@"document.getElementById('fieldid').focus();");
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        return observer_->form_activity_info() != nullptr;
      }));
  EXPECT_EQ(web_state(), observer_->form_activity_info()->web_state);
  EXPECT_EQ(main_frame, observer_->form_activity_info()->sender_frame);
  EXPECT_EQ("form-name",
            observer_->form_activity_info()->form_activity.form_name);
  EXPECT_EQ("text", observer_->form_activity_info()->form_activity.field_type);
  EXPECT_EQ("focus", observer_->form_activity_info()->form_activity.type);
  EXPECT_EQ("", observer_->form_activity_info()->form_activity.value);
  EXPECT_FALSE(observer_->form_activity_info()->form_activity.input_missing);
  EXPECT_TRUE(observer_->form_activity_info()->form_activity.is_main_frame);
  EXPECT_TRUE(observer_->form_activity_info()->form_activity.has_user_gesture);
}

// Tests that observer is called on form removal.
TEST_P(FormActivityTabHelperTest, FormRemovalRegistered) {
  const bool allow_batching = GetParam();

  LoadHtml(@"<form name=\"form1\" id=\"form1\">"
            "<input type=\"text\" name=\"username\" id=\"id1\">"
            "<input type=\"password\" name=\"password\" id=\"id2\">"
            "<input type=\"submit\" id=\"submit_input\"/>"
            "</form>");

  web::WebFrame* main_frame = WaitForMainFrame();
  ASSERT_TRUE(main_frame);
  SetUpForUniqueIds(main_frame);

  ASSERT_FALSE(observer_->form_removal_info());

  TrackFormMutations(main_frame, allow_batching);
  ExecuteJavaScript(@"var form1 = document.getElementById('form1');"
                    @"__gCrWeb.fill.setUniqueIDIfNeeded(form1);"
                    @"form1.parentNode.removeChild(form1);");
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        return observer_->form_removal_info() != nullptr;
      }));

  EXPECT_EQ(web_state(), observer_->form_removal_info()->web_state);
  EXPECT_EQ(main_frame, observer_->form_removal_info()->sender_frame);
  EXPECT_EQ(autofill::FormRendererId(1),
            observer_->form_removal_info()->form_removal_params.unique_form_id);
  EXPECT_FALSE(
      observer_->form_removal_info()->form_removal_params.input_missing);

  if (allow_batching) {
    histogram_tester_.ExpectUniqueSample("Autofill.iOS.FormActivity.DropCount",
                                         /*sample=*/0,
                                         /*expected_bucket_count=*/1);
    histogram_tester_.ExpectUniqueSample("Autofill.iOS.FormActivity.SendCount",
                                         /*sample=*/1,
                                         /*expected_bucket_count=*/1);
    histogram_tester_.ExpectUniqueSample("Autofill.iOS.FormActivity.SendRatio",
                                         /*sample=*/100,
                                         /*expected_bucket_count=*/1);
  } else {
    // Verify that there isn't any metric recorded as the batching feature is
    // disabled.
    histogram_tester_.ExpectTotalCount("Autofill.iOS.FormActivity.DropCount",
                                       0);
    histogram_tester_.ExpectTotalCount("Autofill.iOS.FormActivity.SendCount",
                                       0);
    histogram_tester_.ExpectTotalCount("Autofill.iOS.FormActivity.SendRatio",
                                       0);
  }
}

// Tests that messages that were batched and dropped are correctly recorded as
// such.
TEST_P(FormActivityTabHelperTest,
       RemovedAndAddedFormsRegistered_WithDroppedMessages) {
  const bool allow_batching = GetParam();

  // Basic HTML page with 2 password forms and one formless password form.
  NSString* const html = @"<html><body><form id=\"form1\">"
                          "<input type=\"password\"></form>"
                          "<form id=\"form2\"><input type=\"password\"></form>"
                          "<input id=\"input1\" type=\"password\">"
                          "</body></html>";
  LoadHtml(html);

  web::WebFrame* main_frame = WaitForMainFrame();
  ASSERT_TRUE(main_frame);
  SetUpForUniqueIds(main_frame);

  ASSERT_FALSE(observer_->form_removal_info());
  ASSERT_FALSE(observer_->form_activity_info());

  TrackFormMutations(main_frame, allow_batching);

  // Make a script that batches 2 messages and ignore all other cases once full.
  NSString* const addAndRemoveFormJS =
      @"const parentNode = document.forms[0].parentNode; "
       // Add a generic form and remove a password form, both of which will be
       // notified in the same batch.
       "parentNode.appendChild(document.createElement('form')); "
       "document.getElementById('form1').remove();"
       // Form transformations from here should be ignored.
       // Add non-password form and remove it, 2 notifications dropped.
       "parentNode.appendChild(document.createElement('form')).remove(); "
       // Remove formless password input, 1 notification dropped.
       "document.getElementById('input1').remove();"
       // Remove password form, 1 notification dropped.
       "document.getElementById('form2').remove();";

  ExecuteJavaScript(addAndRemoveFormJS);

  if (allow_batching) {
    // Wait on all the messages in the batch.
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForJSCompletionTimeout, ^bool {
          return observer_->form_removal_info() != nullptr &&
                 observer_->form_activity_info() != nullptr;
        }));

    histogram_tester_.ExpectUniqueSample("Autofill.iOS.FormActivity.DropCount",
                                         /*sample=*/4,
                                         /*expected_bucket_count=*/1);
    histogram_tester_.ExpectUniqueSample("Autofill.iOS.FormActivity.SendCount",
                                         /*sample=*/2,
                                         /*expected_bucket_count=*/1);
    histogram_tester_.ExpectUniqueSample("Autofill.iOS.FormActivity.SendRatio",
                                         /*sample=*/33,
                                         /*expected_bucket_count=*/1);
  } else {
    // Wait on all the messages in the batch.
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForJSCompletionTimeout, ^bool {
          return observer_->form_activity_info() != nullptr;
        }));

    // Verify that there isn't any metric recorded as the batch feature is
    // disabled.
    histogram_tester_.ExpectTotalCount("Autofill.iOS.FormActivity.DropCount",
                                       0);
    histogram_tester_.ExpectTotalCount("Autofill.iOS.FormActivity.SendCount",
                                       0);
    histogram_tester_.ExpectTotalCount("Autofill.iOS.FormActivity.SendRatio",
                                       0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* No InstantiationName */,
    FormActivityTabHelperTest,
    ::testing::Bool());
