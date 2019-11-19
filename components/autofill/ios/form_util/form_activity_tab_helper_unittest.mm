// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/form_activity_tab_helper.h"

#import "base/test/ios/wait_util.h"
#import "components/autofill/ios/form_util/form_activity_observer.h"
#import "components/autofill/ios/form_util/test_form_activity_observer.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/fakes/test_web_state_observer_util.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_js_test.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/platform_test.h"

class FormTestClient : public web::TestWebClient {
 public:
  NSString* GetDocumentStartScriptForAllFrames(
      web::BrowserState* browser_state) const override {
    return web::test::GetPageScript(@"form_util_js");
  }
};

// Tests fixture for autofill::FormActivityTabHelper class.
class FormActivityTabHelperTest
    : public web::WebJsTest<web::WebTestWithWebState> {
 public:
  FormActivityTabHelperTest()
      : web::WebJsTest<web::WebTestWithWebState>(
            std::make_unique<FormTestClient>()) {}

  void SetUp() override {
    web::WebJsTest<web::WebTestWithWebState>::SetUp();
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
    web::WebJsTest<web::WebTestWithWebState>::TearDown();
  }

 protected:
  std::unique_ptr<autofill::TestFormActivityObserver> observer_;
};

// Tests that observer is called on form submission using submit control.
TEST_F(FormActivityTabHelperTest, TestObserverDocumentSubmitted) {
  LoadHtmlAndInject(
      @"<form name='form-name'>"
       "<input type='submit' id='submit'/>"
       "</form>");
  ASSERT_FALSE(observer_->submit_document_info());
  const std::string kTestFormName("form-name");
  const std::string kTestFormData(
      "[{\"name\":\"form-name\",\"origin\":\"https://chromium.test/"
      "\",\"action\":\"https://chromium.test/\","
      "\"name_attribute\":\"form-name\",\"id_attribute\":\"\"}]");
  bool has_user_gesture = false;
  bool form_in_main_frame = true;
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        return web_state()->GetWebFramesManager()->GetMainWebFrame() != nullptr;
      }));
  web::WebFrame* main_frame =
      web_state()->GetWebFramesManager()->GetMainWebFrame();

  ExecuteJavaScript(@"document.getElementById('submit').click();");
  ASSERT_TRUE(observer_->submit_document_info());
  EXPECT_EQ(web_state(), observer_->submit_document_info()->web_state);
  EXPECT_EQ(main_frame, observer_->submit_document_info()->sender_frame);
  EXPECT_EQ(kTestFormName, observer_->submit_document_info()->form_name);
  EXPECT_EQ(kTestFormData, observer_->submit_document_info()->form_data);
  EXPECT_EQ(has_user_gesture,
            observer_->submit_document_info()->has_user_gesture);
  EXPECT_EQ(form_in_main_frame,
            observer_->submit_document_info()->form_in_main_frame);
}

// Tests that observer is called on form submission using submit() method.
TEST_F(FormActivityTabHelperTest, TestFormSubmittedHook) {
  LoadHtmlAndInject(
      @"<form name='form-name' id='form'>"
       "<input type='submit'/>"
       "</form>");
  ASSERT_FALSE(observer_->submit_document_info());
  const std::string kTestFormName("form-name");
  const std::string kTestFormData(
      "[{\"name\":\"form-name\",\"origin\":\"https://chromium.test/"
      "\",\"action\":\"https://chromium.test/\","
      "\"name_attribute\":\"form-name\",\"id_attribute\":\"form\"}]");
  bool has_user_gesture = false;
  bool form_in_main_frame = true;
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        return web_state()->GetWebFramesManager()->GetMainWebFrame() != nullptr;
      }));
  web::WebFrame* main_frame =
      web_state()->GetWebFramesManager()->GetMainWebFrame();

  ExecuteJavaScript(@"document.getElementById('form').submit();");
  ASSERT_TRUE(observer_->submit_document_info());
  EXPECT_EQ(web_state(), observer_->submit_document_info()->web_state);
  EXPECT_EQ(main_frame, observer_->submit_document_info()->sender_frame);
  EXPECT_EQ(kTestFormName, observer_->submit_document_info()->form_name);
  EXPECT_EQ(kTestFormData, observer_->submit_document_info()->form_data);
  EXPECT_EQ(has_user_gesture,
            observer_->submit_document_info()->has_user_gesture);
  EXPECT_EQ(form_in_main_frame,
            observer_->submit_document_info()->form_in_main_frame);
}

// Tests that observer is called on form activity (input event).
TEST_F(FormActivityTabHelperTest, TestObserverFormActivityFrameMessaging) {
  LoadHtmlAndInject(
      @"<form name='form-name'>"
       "<input type='input' name='field-name' id='fieldid'/>"
       "</form>");
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        return web_state()->GetWebFramesManager()->GetMainWebFrame() != nullptr;
      }));
  web::WebFrame* main_frame =
      web_state()->GetWebFramesManager()->GetMainWebFrame();
  ASSERT_FALSE(observer_->form_activity_info());
  // First call will set document.activeElement (which is usually set by user
  // action. Second call will trigger the message.
  ExecuteJavaScript(@"document.getElementById('fieldid').focus();");
  ASSERT_FALSE(observer_->form_activity_info());
  ExecuteJavaScript(@"document.getElementById('fieldid').focus();");
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
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
