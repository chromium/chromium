// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"

#include "components/autofill/ios/form_util/test_form_activity_observer.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakeFormActivityObserver : NSObject<FormActivityObserver>
// Arguments passed to
// |webState:didSubmitDocumentWithFormNamed:withData:userInitiated:inFrame:|.
@property(nonatomic, readonly)
    autofill::TestSubmitDocumentInfo* submitDocumentInfo;
// Arguments passed to
// |webState:didRegisterFormActivity:inFrame:|.
@property(nonatomic, readonly) autofill::TestFormActivityInfo* formActivityInfo;
@end

@implementation FakeFormActivityObserver {
  // Arguments passed to
  // |webState:senderFrame:submittedDocumentWithFormNamed:
  // hasUserGesture:formInMainFrame:inFrame|.
  std::unique_ptr<autofill::TestSubmitDocumentInfo> _submitDocumentInfo;
  // Arguments passed to
  // |webState:senderFrame:didRegisterFormActivity:inFrame:|.
  std::unique_ptr<autofill::TestFormActivityInfo> _formActivityInfo;
}

- (autofill::TestSubmitDocumentInfo*)submitDocumentInfo {
  return _submitDocumentInfo.get();
}

- (autofill::TestFormActivityInfo*)formActivityInfo {
  return _formActivityInfo.get();
}

- (void)webState:(web::WebState*)webState
    didSubmitDocumentWithFormNamed:(const std::string&)formName
                          withData:(const std::string&)formData
                    hasUserGesture:(BOOL)hasUserGesture
                   formInMainFrame:(BOOL)formInMainFrame
                           inFrame:(web::WebFrame*)frame {
  _submitDocumentInfo = std::make_unique<autofill::TestSubmitDocumentInfo>();
  _submitDocumentInfo->web_state = webState;
  _submitDocumentInfo->sender_frame = frame;
  _submitDocumentInfo->form_name = formName;
  _submitDocumentInfo->form_data = formData;
  _submitDocumentInfo->has_user_gesture = hasUserGesture;
  _submitDocumentInfo->form_in_main_frame = formInMainFrame;
}

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  _formActivityInfo = std::make_unique<autofill::TestFormActivityInfo>();
  _formActivityInfo->web_state = webState;
  _formActivityInfo->sender_frame = frame;
  _formActivityInfo->form_activity = params;
}

@end

// Test fixture to test WebStateObserverBridge class.
class FormActivityObserverBridgeTest : public PlatformTest {
 public:
  FormActivityObserverBridgeTest()
      : observer_([[FakeFormActivityObserver alloc] init]),
        observer_bridge_(&test_web_state_, observer_) {}

 protected:
  web::TestWebState test_web_state_;
  FakeFormActivityObserver* observer_;
  autofill::FormActivityObserverBridge observer_bridge_;
};

// Tests |webState:didRegisterFormActivityWithParams:inFrame:| forwarding.
TEST_F(FormActivityObserverBridgeTest, DocumentSubmitted) {
  ASSERT_FALSE([observer_ submitDocumentInfo]);
  std::string kTestFormName("form-name");
  std::string kTestFormData("[]");
  bool has_user_gesture = true;
  bool form_in_main_frame = true;
  web::FakeWebFrame sender_frame("sender_frame", true, GURL::EmptyGURL());
  observer_bridge_.DocumentSubmitted(&test_web_state_, &sender_frame,
                                     kTestFormName, kTestFormData,
                                     has_user_gesture, form_in_main_frame);
  ASSERT_TRUE([observer_ submitDocumentInfo]);
  EXPECT_EQ(&test_web_state_, [observer_ submitDocumentInfo]->web_state);
  EXPECT_EQ(&sender_frame, [observer_ submitDocumentInfo]->sender_frame);
  EXPECT_EQ(kTestFormName, [observer_ submitDocumentInfo]->form_name);
  EXPECT_EQ(kTestFormData, [observer_ submitDocumentInfo]->form_data);
  EXPECT_EQ(has_user_gesture, [observer_ submitDocumentInfo]->has_user_gesture);
  EXPECT_EQ(form_in_main_frame,
            [observer_ submitDocumentInfo]->form_in_main_frame);
}

// Tests |webState:didRegisterFormActivity:...| forwarding.
TEST_F(FormActivityObserverBridgeTest, FormActivityRegistered) {
  ASSERT_FALSE([observer_ formActivityInfo]);

  autofill::FormActivityParams params;
  web::FakeWebFrame sender_frame("sender_frame", true, GURL::EmptyGURL());
  params.form_name = "form-name";
  params.field_type = "field-type";
  params.type = "type";
  params.value = "value";
  params.input_missing = true;
  observer_bridge_.FormActivityRegistered(&test_web_state_, &sender_frame,
                                          params);
  ASSERT_TRUE([observer_ formActivityInfo]);
  EXPECT_EQ(&test_web_state_, [observer_ formActivityInfo]->web_state);
  EXPECT_EQ(&sender_frame, [observer_ formActivityInfo]->sender_frame);
  EXPECT_EQ(params.form_name,
            [observer_ formActivityInfo]->form_activity.form_name);
  EXPECT_EQ(params.field_type,
            [observer_ formActivityInfo]->form_activity.field_type);
  EXPECT_EQ(params.type, [observer_ formActivityInfo]->form_activity.type);
  EXPECT_EQ(params.value, [observer_ formActivityInfo]->form_activity.value);
  EXPECT_TRUE([observer_ formActivityInfo]->form_activity.input_missing);
}
