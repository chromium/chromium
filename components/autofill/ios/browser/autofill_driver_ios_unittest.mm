// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_driver_ios.h"

#import <memory>
#import <optional>

#import "base/test/mock_callback.h"
#import "base/test/test_future.h"
#import "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_test_utils.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/form_field_data.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_test.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"

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

    GURL url("https://example.com");
    web_state_.SetCurrentURL(url);
    web_state_.SetBrowserState(GetBrowserState());
    web_state_.SetContentIsHTML(true);

    // Define frames managers for both content worlds so that the
    // `ChildFrameRegistrar` below can be created.
    for (auto content_world : {web::ContentWorld::kIsolatedWorld,
                               web::ContentWorld::kPageContentWorld}) {
      auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
      web_state_.SetWebFramesManager(content_world, std::move(frames_manager));
    }

    web_frames_manager_ = static_cast<web::FakeWebFramesManager*>(
        web_state_.GetWebFramesManager(AutofillJavaScriptFeature::GetInstance()
                                           ->GetSupportedContentWorld()));

    web::WebFrame* main_frame = CreateFrame(url, /*main_frame=*/true);
    web::WebFrame* iframe = CreateFrame(url, /*main_frame=*/false);

    // Create the main Autofill classes.
    test_bridge_ = [[FakeAutofillDriverIOSBridge alloc] init];
    autofill_client_ =
        std::make_unique<TestAutofillClientIOS>(&web_state_, test_bridge_);
    main_frame_driver_ =
        AutofillDriverIOS::FromWebStateAndWebFrame(&web_state_, main_frame);
    iframe_driver_ =
        AutofillDriverIOS::FromWebStateAndWebFrame(&web_state_, iframe);

    ASSERT_TRUE(main_frame_driver_ && main_frame_driver_->GetFrameToken());
    ASSERT_TRUE(iframe_driver_ && iframe_driver_->GetFrameToken());
    ASSERT_NE(iframe_driver_->GetFrameToken(),
              main_frame_driver_->GetFrameToken());

    // Register the remote-local frame token mapping for the iframe driver so
    // that the main frame driver can accurately set itself as a parent of the
    // iframe driver.
    registrar()->RegisterMapping(GetRemoteFrameToken(iframe_driver_),
                                 iframe_driver_->GetFrameToken());
  }

  AutofillDriverIOS* main_frame_driver() { return main_frame_driver_; }
  AutofillDriverIOS* iframe_driver() { return iframe_driver_; }
  FakeAutofillDriverIOSBridge* bridge() { return test_bridge_; }

  FormData MakeForm(bool main_frame) {
    FormGlobalId form_id{main_frame ? main_frame_driver()->GetFrameToken()
                                    : iframe_driver()->GetFrameToken(),
                         test::MakeFormRendererId()};
    FormData form;
    form.set_renderer_id(form_id.renderer_id);
    form.set_host_frame(form_id.frame_token);

    // Add a field to the form to make it non-trivial.
    FormFieldData field = test::GetFormFieldData({});
    field.set_host_form_id(form.renderer_id());
    field.set_host_frame(form.host_frame());
    field.set_value(main_frame ? u"Main frame field" : u"Iframe field");
    form.set_fields({std::move(field)});

    if (main_frame) {
      std::vector<FrameTokenWithPredecessor> child_frames(1);
      child_frames.back().token = GetRemoteFrameToken(iframe_driver());
      form.set_child_frames(std::move(child_frames));
    }
    return form;
  }

 private:
  web::WebFrame* CreateFrame(GURL url, bool main_frame) {
    auto frame = main_frame ? web::FakeWebFrame::CreateMainWebFrame(url)
                            : web::FakeWebFrame::CreateChildWebFrame(url);
    frame->set_browser_state(GetBrowserState());
    web::WebFrame* frame_ptr = frame.get();
    web_frames_manager_->AddWebFrame(std::move(frame));
    return frame_ptr;
  }

  autofill::ChildFrameRegistrar* registrar() {
    return autofill::ChildFrameRegistrar::GetOrCreateForWebState(&web_state_);
  }

  RemoteFrameToken GetRemoteFrameToken(AutofillDriver* driver) {
    return RemoteFrameToken(driver->GetFrameToken().value());
  }

  test::AutofillUnitTestEnvironment autofill_test_environment_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeWebFramesManager> web_frames_manager_ = nullptr;
  FakeAutofillDriverIOSBridge* test_bridge_ = nullptr;
  std::unique_ptr<TestAutofillClientIOS> autofill_client_;
  raw_ptr<AutofillDriverIOS> main_frame_driver_ = nullptr;
  raw_ptr<AutofillDriverIOS> iframe_driver_ = nullptr;
};

// Test when ExtractFormWithField() is called and where the form is found.
TEST_F(AutofillDriverIOSTest, ExtractFormWithField_FormFound) {
  FormData iframe_form = MakeForm(/*main_frame=*/false);
  FormData main_frame_form = MakeForm(/*main_frame=*/true);

  // Notify the drivers of seeing these forms so that the router can register
  // them along with their frame.
  main_frame_driver()->FormsSeen({main_frame_form}, /*removed_forms=*/{});
  iframe_driver()->FormsSeen({iframe_form}, /*removed_forms=*/{});
  ASSERT_EQ(iframe_driver()->GetParent(), main_frame_driver());

  // Update the value of the fields in both forms to check the result of the
  // extraction later.
  auto update_form = [](FormData& form) {
    std::vector<FormFieldData> fields = form.ExtractFields();
    fields.back().set_value(fields.back().value() + u" extracted");
    form.set_fields(std::move(fields));
  };
  update_form(main_frame_form);
  update_form(iframe_form);

  // Setup the forms that the bridge should return upon calling
  // fetchFormsFiltered(). Those should be the two forms created above.
  std::vector<FormData> bridge_result{iframe_form, main_frame_form};
  [bridge() setForms:(bridge_result)];

  base::MockCallback<
      base::OnceCallback<void(AutofillDriver*, const std::optional<FormData>&)>>
      final_callback;
  // The callback must be called with the result of the extraction operation,
  // this should be the browser form containing the iframe form, with the
  // updated version of the iframe form.
  EXPECT_CALL(
      final_callback,
      Run(main_frame_driver(),
          Optional(
              AllOf(Property(&FormData::global_id, main_frame_form.global_id()),
                    Property(&FormData::fields,
                             ElementsAre(Property(&FormFieldData::value,
                                                  u"Iframe field extracted"),
                                         Property(&FormFieldData::value,
                                                  u"Main frame field")))))));
  main_frame_driver()->ExtractFormWithField(iframe_form.fields()[0].global_id(),
                                            final_callback.Get());
}

// Test the case where the bridge returns forms, but not the one we want.
TEST_F(AutofillDriverIOSTest, ExtractFormWithField_FormNotFound) {
  FieldGlobalId field_id{main_frame_driver()->GetFrameToken(),
                         test::MakeFieldRendererId()};
  FormData form_not_to_find = MakeForm(/*main_frame=*/true);

  // Setup the forms that the bridge should return upon calling
  // fetchFormsFiltered().
  std::vector<FormData> bridge_result = {form_not_to_find};
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
  main_frame_driver()->ExtractFormWithField(field_id, final_callback.Get());
}

}  // namespace

}  // namespace autofill
