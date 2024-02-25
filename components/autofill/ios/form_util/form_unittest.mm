// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "components/autofill/ios/form_util/autofill_test_with_web_state.h"
#import "components/autofill/ios/form_util/form_activity_tab_helper.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "components/autofill/ios/form_util/test_form_activity_observer.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using autofill::FieldRendererId;
using autofill::FormRendererId;
using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;

// Text fixture to test password controller.
class FormJsTest : public AutofillTestWithWebState {
 public:
  FormJsTest()
      : AutofillTestWithWebState(std::make_unique<web::FakeWebClient>()) {
    web::FakeWebClient* web_client =
        static_cast<web::FakeWebClient*>(GetWebClient());
    web_client->SetJavaScriptFeatures(
        {autofill::FormUtilJavaScriptFeature::GetInstance(),
         autofill::FormHandlersJavaScriptFeature::GetInstance()});
  }

  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    observer_ =
        std::make_unique<autofill::TestFormActivityObserver>(web_state());
    autofill::FormActivityTabHelper::GetOrCreateForWebState(web_state())
        ->AddObserver(observer_.get());
  }

  void TearDown() override {
    autofill::FormActivityTabHelper::GetOrCreateForWebState(web_state())
        ->RemoveObserver(observer_.get());
    web::WebTestWithWebState::TearDown();
  }

 protected:
  web::WebFrame* WaitForMainFrame() {
    __block web::WebFrame* main_frame = nullptr;
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
      web::WebFramesManager* frames_manager =
          autofill::FormUtilJavaScriptFeature::GetInstance()
              ->GetWebFramesManager(web_state());
      main_frame = frames_manager->GetMainWebFrame();
      return main_frame != nullptr;
    }));
    return main_frame;
  }

  std::unique_ptr<autofill::TestFormActivityObserver> observer_;
};

// Tests that keyup event correctly delivered to WebStateObserver if the element
// is focused.
TEST_F(FormJsTest, KeyUpEventFocused) {
  LoadHtml(@"<p><input id='test'/></p>");
  ASSERT_FALSE(observer_->form_activity_info());
  ExecuteJavaScript(
      @"var e = document.getElementById('test');"
       "e.focus();"
       "var ev = new KeyboardEvent('keyup', {bubbles:true});"
       "e.dispatchEvent(ev);");
  autofill::TestFormActivityObserver* block_observer = observer_.get();
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return block_observer->form_activity_info() != nullptr;
  }));
  autofill::TestFormActivityInfo* info = observer_->form_activity_info();
  ASSERT_TRUE(info);
  EXPECT_EQ("keyup", info->form_activity.type);
  EXPECT_FALSE(info->form_activity.input_missing);
}

// Tests that keyup event is not delivered to WebStateObserver if the element is
// not focused.
TEST_F(FormJsTest, KeyUpEventNotFocused) {
  LoadHtml(@"<p><input id='test'/></p>");
  ASSERT_FALSE(observer_->form_activity_info());
  ExecuteJavaScript(
      @"var e = document.getElementById('test');"
       "var ev = new KeyboardEvent('keyup', {bubbles:true});"
       "e.dispatchEvent(ev);");
  WaitForBackgroundTasks();
  autofill::TestFormActivityInfo* info = observer_->form_activity_info();
  ASSERT_FALSE(info);
}

// Tests that focus event correctly delivered to WebStateObserver.
TEST_F(FormJsTest, FocusMainFrame) {
  LoadHtml(
      @"<form>"
       "<input type='text' name='username' id='id1'>"
       "<input type='password' name='password' id='id2'>"
       "</form>");
  ASSERT_FALSE(observer_->form_activity_info());
  ExecuteJavaScript(@"document.getElementById('id1').focus();");
  autofill::TestFormActivityObserver* block_observer = observer_.get();
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return block_observer->form_activity_info() != nullptr;
  }));
  autofill::TestFormActivityInfo* info = observer_->form_activity_info();
  ASSERT_TRUE(info);
  EXPECT_EQ("focus", info->form_activity.type);
  EXPECT_FALSE(info->form_activity.input_missing);
}

// Tests that submit event correctly delivered to WebStateObserver.
TEST_F(FormJsTest, FormSubmitMainFrame) {
  LoadHtml(
      @"<form id='form1'>"
       "<input type='password'>"
       "<input type='submit' id='submit_input'/>"
       "</form>");
  ASSERT_FALSE(observer_->submit_document_info());
  ExecuteJavaScript(@"document.getElementById('submit_input').click();");
  autofill::TestSubmitDocumentInfo* info = observer_->submit_document_info();
  ASSERT_TRUE(info);
  EXPECT_EQ("form1", info->form_name);
}

// Tests that focus event from same-origin iframe correctly delivered to
// WebStateObserver.
TEST_F(FormJsTest, FocusSameOriginIFrame) {
  LoadHtml(@"<iframe id='frame1'></iframe>");
  ExecuteJavaScript(
      @"document.getElementById('frame1').contentDocument.body.innerHTML = "
       "'<form>"
       "<input type=\"text\" name=\"username\" id=\"id1\">"
       "<input type=\"password\" name=\"password\" id=\"id2\">"
       "</form>'");

  ExecuteJavaScript(
      @"document.getElementById('frame1').contentDocument.getElementById('id1')"
      @".focus()");
  autofill::TestFormActivityObserver* block_observer = observer_.get();
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return block_observer->form_activity_info() != nullptr;
  }));
  autofill::TestFormActivityInfo* info = observer_->form_activity_info();
  ASSERT_TRUE(info);
  EXPECT_EQ("focus", info->form_activity.type);
  EXPECT_FALSE(info->form_activity.input_missing);
}

// Tests that submit event from same-origin iframe correctly delivered to
// WebStateObserver.
TEST_F(FormJsTest, FormSameOriginIFrame) {
  LoadHtml(@"<iframe id='frame1'></iframe>");
  ExecuteJavaScript(
      @"document.getElementById('frame1').contentDocument.body.innerHTML = "
       "'<form id=\"form1\">"
       "<input type=\"password\" name=\"password\" id=\"id2\">"
       "<input type=\"submit\" id=\"submit_input\"/>"
       "</form>'");
  ExecuteJavaScript(
      @"document.getElementById('frame1').contentDocument.getElementById('"
      @"submit_input').click();");
  autofill::TestSubmitDocumentInfo* info = observer_->submit_document_info();
  ASSERT_TRUE(info);
  EXPECT_EQ("form1", info->form_name);
}

// Tests that a new form triggers form_changed event.
TEST_F(FormJsTest, AddForm) {
  LoadHtml(@"<body></body>");

  web::WebFrame* main_frame = WaitForMainFrame();
  ASSERT_TRUE(main_frame);
  SetUpForUniqueIds(main_frame);
  TrackFormMutations(main_frame);

  ExecuteJavaScript(
      @"var form = document.createElement('form');"
      @"document.body.appendChild(form);");
  autofill::TestFormActivityObserver* block_observer = observer_.get();
  __block autofill::TestFormActivityInfo* info = nil;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    info = block_observer->form_activity_info();
    return info != nil;
  }));
  EXPECT_EQ("form_changed", info->form_activity.type);
  EXPECT_FALSE(info->form_activity.input_missing);
}

// Tests that a new input element triggers form_changed event.
TEST_F(FormJsTest, AddInput) {
  LoadHtml(@"<form id='formId'/>");

  web::WebFrame* main_frame = WaitForMainFrame();
  ASSERT_TRUE(main_frame);
  TrackFormMutations(main_frame);

  ExecuteJavaScript(
      @"var input = document.createElement('input');"
      @"document.getElementById('formId').appendChild(input);");
  autofill::TestFormActivityObserver* block_observer = observer_.get();
  __block autofill::TestFormActivityInfo* info = nil;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    info = block_observer->form_activity_info();
    return info != nil;
  }));
  EXPECT_EQ("form_changed", info->form_activity.type);
  EXPECT_FALSE(info->form_activity.input_missing);
}

// Tests that a new select element triggers form_changed event.
TEST_F(FormJsTest, AddSelect) {
  LoadHtml(@"<form id='formId'/>");

  web::WebFrame* main_frame = WaitForMainFrame();
  ASSERT_TRUE(main_frame);
  TrackFormMutations(main_frame);

  ExecuteJavaScript(
      @"var select = document.createElement('select');"
      @"document.getElementById('formId').appendChild(select);");
  autofill::TestFormActivityObserver* block_observer = observer_.get();
  __block autofill::TestFormActivityInfo* info = nil;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    info = block_observer->form_activity_info();
    return info != nil;
  }));
  EXPECT_EQ("form_changed", info->form_activity.type);
  EXPECT_FALSE(info->form_activity.input_missing);
}

// Tests that a new option element triggers form_changed event.
TEST_F(FormJsTest, AddOption) {
  LoadHtml(
      @"<form>"
       "<select id='select1'><option value='CA'>CA</option></select>"
       "</form>");

  web::WebFrame* main_frame = WaitForMainFrame();
  ASSERT_TRUE(main_frame);
  TrackFormMutations(main_frame);

  ExecuteJavaScript(
      @"var option = document.createElement('option');"
      @"document.getElementById('select1').appendChild(option);");
  autofill::TestFormActivityObserver* block_observer = observer_.get();
  __block autofill::TestFormActivityInfo* info = nil;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    info = block_observer->form_activity_info();
    return info != nil;
  }));
  EXPECT_EQ("form_changed", info->form_activity.type);
  EXPECT_FALSE(info->form_activity.input_missing);
}

// Tests that removing password form triggers 'password_form_removed" event.
TEST_F(FormJsTest, RemoveForm) {
  LoadHtml(@"<form id=\"form1\">"
            "<input type=\"text\" name=\"username\" id=\"id1\">"
            "<input type=\"password\" name=\"password\" id=\"id2\">"
            "<input type=\"submit\" id=\"submit_input\"/>"
            "</form>");

  web::WebFrame* main_frame = WaitForMainFrame();
  ASSERT_TRUE(main_frame);
  SetUpForUniqueIds(main_frame);
  TrackFormMutations(main_frame);

  ExecuteJavaScript(@"var form1 = document.getElementById('form1');"
                    @"__gCrWeb.fill.setUniqueIDIfNeeded(form1);"
                    @"form1.parentNode.removeChild(form1);");
  autofill::TestFormActivityObserver* block_observer = observer_.get();
  __block autofill::TestFormRemovalInfo* info = nil;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    info = block_observer->form_removal_info();
    return info != nil;
  }));
  EXPECT_FALSE(info->form_removal_params.input_missing);
  EXPECT_EQ(FormRendererId(1), info->form_removal_params.unique_form_id);
}

// Tests that removing unowned password fields triggers 'password_form_removed"
// event.
TEST_F(FormJsTest, RemoveFormlessPasswordFields) {
  LoadHtml(@"<body><div>"
            "<input type=\"password\" name=\"password\" id=\"pw\">"
            "<input type=\"submit\" id=\"submit_input\"/>"
            "</div></body>");

  web::WebFrame* main_frame = WaitForMainFrame();
  ASSERT_TRUE(main_frame);
  SetUpForUniqueIds(main_frame);
  TrackFormMutations(main_frame);

  ExecuteJavaScript(@"var password = document.getElementById('pw');"
                    @"__gCrWeb.fill.setUniqueIDIfNeeded(password);"
                    @"password.parentNode.removeChild(password);");

  autofill::TestFormActivityObserver* block_observer = observer_.get();
  __block autofill::TestFormRemovalInfo* info = nil;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    info = block_observer->form_removal_info();
    return info != nil;
  }));
  EXPECT_FALSE(info->form_removal_params.input_missing);
  EXPECT_FALSE(info->form_removal_params.unique_form_id);
  std::vector<FieldRendererId> expected_removed_ids = {FieldRendererId(1)};
  EXPECT_EQ(info->form_removal_params.removed_unowned_fields,
            expected_removed_ids);
}

// Tests that a new element that contains 'form' in the tag name does not
// trigger a form_changed event.
TEST_F(FormJsTest, AddCustomElement) {
  LoadHtml(@"<body></body>");
  web::WebFrame* main_frame = WaitForMainFrame();
  ASSERT_TRUE(main_frame);
  TrackFormMutations(main_frame);

  ExecuteJavaScript(@"var form = document.createElement('my-form');"
                    @"document.body.appendChild(form);");

  // Check that no activity is observed upon JS completion.
  autofill::TestFormActivityObserver* block_observer = observer_.get();
  __block autofill::TestFormActivityInfo* info = nil;
  EXPECT_FALSE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    info = block_observer->form_activity_info();
    return info != nil;
  }));
}

TEST_F(FormJsTest, GetIframeElements) {
  LoadHtml(@"<iframe id='frame1' srcdoc='foo'></iframe>"
           @"<p id='not-an-iframe'>"
           @"<iframe id='frame2' srcdoc='bar'></iframe>"
           @"<marquee id='definitely-not-an-iframe'>baz</marquee>"
           @"</p>");

  web::WebFrame* main_frame = WaitForMainFrame();
  ASSERT_TRUE(main_frame);

  // Check that the right elements were found.
  EXPECT_NSEQ(
      @"frame1,frame2",
      ExecuteJavaScript(
          @"const frames = __gCrWeb.form.getIframeElements(document.body);"
          @"frames.map((f) => { return f.id; }).join();"));

  // Check that the return objects have a truthy contentWindow property.
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"!!(frames[0].contentWindow);"));
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"!!(frames[1].contentWindow);"));
}
