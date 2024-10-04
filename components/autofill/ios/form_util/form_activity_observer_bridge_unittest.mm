// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"

#import "components/autofill/core/common/form_data.h"
#import "components/autofill/ios/form_util/test_form_activity_observer.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

@interface FakeFormActivityObserver : NSObject<FormActivityObserver>
// Arguments passed to
// |webState:didSubmitDocumentWithFormData:userInitiated:inFrame:|.
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
  // Arguments passed to
  // |webState:senderFrame:didRegisterFormRemoval:inFrame:|.
  std::unique_ptr<autofill::TestFormRemovalInfo> _formRemovalInfo;
}

- (autofill::TestSubmitDocumentInfo*)submitDocumentInfo {
  return _submitDocumentInfo.get();
}

- (autofill::TestFormActivityInfo*)formActivityInfo {
  return _formActivityInfo.get();
}

- (autofill::TestFormRemovalInfo*)formRemovalInfo {
  return _formRemovalInfo.get();
}

- (void)webState:(web::WebState*)webState
    didSubmitDocumentWithFormData:(const autofill::FormData&)formData
                   hasUserGesture:(BOOL)hasUserGesture
                          inFrame:(web::WebFrame*)frame {
  _submitDocumentInfo = std::make_unique<autofill::TestSubmitDocumentInfo>();
  _submitDocumentInfo->web_state = webState;
  _submitDocumentInfo->sender_frame = frame;
  _submitDocumentInfo->form_data = formData;
  _submitDocumentInfo->has_user_gesture = hasUserGesture;
}

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  _formActivityInfo = std::make_unique<autofill::TestFormActivityInfo>();
  _formActivityInfo->web_state = webState;
  _formActivityInfo->sender_frame = frame;
  _formActivityInfo->form_activity = params;
}

- (void)webState:(web::WebState*)webState
    didRegisterFormRemoval:(const autofill::FormRemovalParams&)params
                   inFrame:(web::WebFrame*)frame {
  _formRemovalInfo = std::make_unique<autofill::TestFormRemovalInfo>();
  _formRemovalInfo->web_state = webState;
  _formRemovalInfo->sender_frame = frame;
  _formRemovalInfo->form_removal_params = params;
}

@end

// Test fixture to test WebStateObserverBridge class.
class FormActivityObserverBridgeTest : public PlatformTest {
 public:
  FormActivityObserverBridgeTest()
      : observer_([[FakeFormActivityObserver alloc] init]),
        observer_bridge_(&fake_web_state_, observer_) {}

 protected:
  web::FakeWebState fake_web_state_;
  FakeFormActivityObserver* observer_;
  autofill::FormActivityObserverBridge observer_bridge_;
};

// Tests |webState:didRegisterFormActivityWithParams:inFrame:| forwarding.
TEST_F(FormActivityObserverBridgeTest, DocumentSubmitted) {
  ASSERT_FALSE([observer_ submitDocumentInfo]);
  autofill::FormData kTestFormData;
  bool has_user_gesture = true;
  auto sender_frame = web::FakeWebFrame::Create("sender_frame", true, GURL());
  observer_bridge_.DocumentSubmitted(&fake_web_state_, sender_frame.get(),
                                     kTestFormData, has_user_gesture);
  ASSERT_TRUE([observer_ submitDocumentInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ submitDocumentInfo]->web_state);
  EXPECT_EQ(sender_frame.get(), [observer_ submitDocumentInfo]->sender_frame);
  EXPECT_EQ(kTestFormData, [observer_ submitDocumentInfo]->form_data);
  EXPECT_EQ(has_user_gesture, [observer_ submitDocumentInfo]->has_user_gesture);
}

// Tests |webState:didRegisterFormActivity:...| forwarding.
TEST_F(FormActivityObserverBridgeTest, FormActivityRegistered) {
  ASSERT_FALSE([observer_ formActivityInfo]);

  autofill::FormActivityParams params;
  auto sender_frame = web::FakeWebFrame::Create("sender_frame", true, GURL());
  params.form_name = "form-name";
  params.field_type = "field-type";
  params.type = "type";
  params.value = "value";
  params.input_missing = true;
  observer_bridge_.FormActivityRegistered(&fake_web_state_, sender_frame.get(),
                                          params);
  ASSERT_TRUE([observer_ formActivityInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ formActivityInfo]->web_state);
  EXPECT_EQ(sender_frame.get(), [observer_ formActivityInfo]->sender_frame);
  EXPECT_EQ(params.form_name,
            [observer_ formActivityInfo]->form_activity.form_name);
  EXPECT_EQ(params.field_type,
            [observer_ formActivityInfo]->form_activity.field_type);
  EXPECT_EQ(params.type, [observer_ formActivityInfo]->form_activity.type);
  EXPECT_EQ(params.value, [observer_ formActivityInfo]->form_activity.value);
  EXPECT_TRUE([observer_ formActivityInfo]->form_activity.input_missing);
}

// Tests |webState:didRegisterFormRemoval:...| forwarding.
TEST_F(FormActivityObserverBridgeTest, FormRemovalRegistered) {
  ASSERT_FALSE([observer_ formRemovalInfo]);

  autofill::FormRemovalParams params;
  auto sender_frame = web::FakeWebFrame::Create("sender_frame", true, GURL());
  params.removed_forms = {autofill::FormRendererId(1)};
  observer_bridge_.FormRemoved(&fake_web_state_, sender_frame.get(), params);
  ASSERT_TRUE([observer_ formRemovalInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ formRemovalInfo]->web_state);
  EXPECT_EQ(sender_frame.get(), [observer_ formRemovalInfo]->sender_frame);
  EXPECT_EQ(params.removed_forms,
            [observer_ formRemovalInfo]->form_removal_params.removed_forms);
}
