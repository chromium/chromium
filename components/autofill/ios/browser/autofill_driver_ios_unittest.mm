// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_driver_ios.h"

#import <memory>
#import <optional>
#import <string_view>

#import "base/test/mock_callback.h"
#import "base/test/test_future.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_test_utils.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/test/web_test.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// AutofillDriverIosBridge used for testing. Provides a simple implementation of
// the methods that are used during testing, e.g. call the completion block upon
// calling -fetchFormsFiltered.
@interface FakeAutofillDriverIOSBridge : NSObject <AutofillDriverIOSBridge>

- (instancetype)init;
- (void)setForms:(std::vector<autofill::FormData>)forms;

@end

@implementation FakeAutofillDriverIOSBridge {
  std::vector<autofill::FormData> _forms;
}

- (instancetype)init {
  if ((self = [super init])) {
    _forms = {};
  }
  return self;
}

- (void)setForms:(std::vector<autofill::FormData>)forms {
  _forms = std::move(forms);
}

- (void)fillData:(const std::vector<autofill::FormFieldData::FillData>&)fields
         section:(const autofill::Section&)section
         inFrame:(web::WebFrame*)frame {
}
- (void)fillSpecificFormField:(const autofill::FieldRendererId&)field
                    withValue:(const std::u16string)value
                      inFrame:(web::WebFrame*)frame {
}
- (void)handleParsedForms:
            (const std::vector<
                raw_ptr<autofill::FormStructure, VectorExperimental>>&)forms
                  inFrame:(web::WebFrame*)frame {
}
- (void)fillFormDataPredictions:
            (const std::vector<autofill::FormDataPredictions>&)forms
                        inFrame:(web::WebFrame*)frame {
}
- (void)scanFormsInWebState:(web::WebState*)webState
                    inFrame:(web::WebFrame*)webFrame {
}
- (void)notifyFormsSeen:(const std::vector<autofill::FormData>&)updatedForms
                inFrame:(web::WebFrame*)frame {
}
- (void)fetchFormsFiltered:(BOOL)filtered
                  withName:(const std::u16string&)formName
                   inFrame:(web::WebFrame*)frame
         completionHandler:(FormFetchCompletion)completionHandler {
  std::move(completionHandler).Run(_forms);
}

@end

namespace autofill {

namespace {

using testing::Eq;
using testing::Optional;
using testing::Property;

class AutofillDriverIOSTest : public web::WebTest {
 protected:
  void SetUp() override {
    web::WebTest::SetUp();

    OverrideJavaScriptFeatures(
        {autofill::AutofillJavaScriptFeature::GetInstance()});

    web_state_.SetBrowserState(GetBrowserState());
    web_state_.SetContentIsHTML(true);

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_state_.SetWebFramesManager(
        AutofillJavaScriptFeature::GetInstance()->GetSupportedContentWorld(),
        std::move(frames_manager));
    web_frames_manager_ = static_cast<web::FakeWebFramesManager*>(
        web_state_.GetWebFramesManager(AutofillJavaScriptFeature::GetInstance()
                                           ->GetSupportedContentWorld()));

    GURL url("https://example.com");
    web_state_.SetCurrentURL(url);

    // Create the main frame.
    auto main_frame = web::FakeWebFrame::CreateMainWebFrame(url);
    main_frame->set_browser_state(GetBrowserState());
    web::WebFrame* main_frame_ptr = main_frame.get();
    web_frames_manager_->AddWebFrame(std::move(main_frame));

    test_bridge_ = [[FakeAutofillDriverIOSBridge alloc] init];

    autofill_client_ =
        std::make_unique<TestAutofillClientIOS>(&web_state_, test_bridge_);
    main_frame_driver_ =
        AutofillDriverIOS::FromWebStateAndWebFrame(&web_state_, main_frame_ptr);
    ASSERT_TRUE(main_frame_driver_ && main_frame_driver_->GetFrameToken());
  }

  AutofillDriverIOS* main_frame_driver() { return main_frame_driver_; }
  FakeAutofillDriverIOSBridge* bridge() { return test_bridge_; }

  FormGlobalId MakeFormId() {
    return {main_frame_driver()->GetFrameToken(), test::MakeFormRendererId()};
  }

  FormData MakeForm() {
    FormGlobalId form_id = MakeFormId();
    FormData form;
    form.set_renderer_id(form_id.renderer_id);
    form.set_host_frame(form_id.frame_token);
    return form;
  }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeWebFramesManager> web_frames_manager_ = nullptr;
  FakeAutofillDriverIOSBridge* test_bridge_ = nullptr;
  std::unique_ptr<TestAutofillClientIOS> autofill_client_;
  raw_ptr<AutofillDriverIOS> main_frame_driver_ = nullptr;
};

// Test when ExtractForm() is called and where the form is found.
TEST_F(AutofillDriverIOSTest, ExtractForm_FormFound) {
  FormData form_to_find = MakeForm();
  FormData form_not_to_find = MakeForm();
  std::vector<FormData> bridge_result = {form_to_find, form_not_to_find};

  // Setup the forms that the bridge should return upon calling
  // fetchFormsFiltered(). Those should be the two forms created above.
  [bridge() setForms:(bridge_result)];

  // Notify the driver of seeing these forms so that the router can register
  // them along with their frame.
  main_frame_driver()->FormsSeen({form_to_find, form_not_to_find},
                                 /*removed_forms=*/{});

  // Expect that `final_callback` will be called with a form with the correct
  // renderer id.
  base::MockCallback<
      base::OnceCallback<void(AutofillDriver*, const std::optional<FormData>&)>>
      final_callback;
  EXPECT_CALL(final_callback,
              Run(main_frame_driver(),
                  Optional(Property(&FormData::renderer_id,
                                    form_to_find.global_id().renderer_id))));
  main_frame_driver()->ExtractForm(form_to_find.global_id(),
                                   final_callback.Get());
}

// Test the case where the bridge returns forms, but not the one we want.
TEST_F(AutofillDriverIOSTest, ExtractForm_FormNotFound) {
  FormGlobalId form_id = MakeFormId();
  FormData form_not_to_find = MakeForm();

  std::vector<FormData> bridge_result = {form_not_to_find};

  // Setup the forms that the bridge should return upon calling
  // fetchFormsFiltered(). This should be the form created above with
  // `other_form_id`.
  [bridge() setForms:(bridge_result)];

  // Notify the driver of seeing this form so that the router can register it
  // along with its frame.
  main_frame_driver()->FormsSeen({form_not_to_find}, /*removed_forms=*/{});

  // Expect that `final_callback` will be called with a null form, since the
  // bridge couldn't find a form with the corresponding renderer id.
  base::MockCallback<
      base::OnceCallback<void(AutofillDriver*, const std::optional<FormData>&)>>
      final_callback;
  EXPECT_CALL(final_callback, Run(nullptr, Eq(std::nullopt)));
  main_frame_driver()->ExtractForm(form_id, final_callback.Get());
}

}  // namespace

}  // namespace autofill
