// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_form_helper.h"

#import <stddef.h>

#import "base/debug/crash_logging.h"
#import "base/debug/dump_without_crashing.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "components/autofill/core/common/field_data_manager.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/password_form_fill_data.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/common/field_data_manager_factory_ios.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "components/password_manager/ios/account_select_fill_data.h"
#import "components/password_manager/ios/ios_password_manager_driver.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/password_manager/ios/password_manager_ios_util.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "components/password_manager/ios/password_manager_tab_helper.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "services/metrics/public/cpp/ukm_builders.h"

using autofill::FieldPropertiesFlags;
using autofill::FormData;
using autofill::FormRendererId;
using autofill::FieldRendererId;
using autofill::PasswordFormFillData;
using base::SysNSStringToUTF16;
using base::UTF16ToUTF8;
using password_manager::FillData;
using password_manager::JsonStringToFormData;

namespace password_manager {

// The frame id associated with the frame which sent to form message.
const char kHostFrameKey[] = "host_frame";

}  // namespace password_manager

@interface PasswordFormHelper ()

// Parses the |jsonString| which contatins the password forms found on a web
// page to populate the |forms| vector.
- (void)getPasswordForms:(std::vector<FormData>*)forms
                fromJSON:(NSString*)jsonString
                 pageURL:(const GURL&)pageURL
                   frame:(web::WebFrame*)frame;

// Records both UMA & UKM metrics.
- (void)recordFormFillingSuccessMetrics:(bool)success;

@end

@implementation PasswordFormHelper {
  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  raw_ptr<web::WebState> _webState;

  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe form activity in |_webState|.
  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;
}

#pragma mark - Properties

- (const GURL&)lastCommittedURL {
  return _webState ? _webState->GetLastCommittedURL() : GURL::EmptyGURL();
}

#pragma mark - Initialization

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webState = webState;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    _formActivityObserverBridge =
        std::make_unique<autofill::FormActivityObserverBridge>(_webState, self);

    password_manager::PasswordManagerTabHelper::GetOrCreateForWebState(webState)
        ->SetFormHelper(self);
  }
  return self;
}

#pragma mark - Dealloc

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webState = nullptr;
  }
  _webStateObserverBridge.reset();
  _formActivityObserverBridge.reset();
}

#pragma mark - FormActivityObserver

- (void)webState:(web::WebState*)webState
    didSubmitDocumentWithFormData:(const FormData&)formData
                   hasUserGesture:(BOOL)hasUserGesture
                          inFrame:(web::WebFrame*)frame {
  DCHECK_EQ(_webState, webState);
  GURL pageURL = webState->GetLastCommittedURL();
  if (pageURL.DeprecatedGetOriginAsURL() != frame->GetSecurityOrigin()) {
    // Passwords is only supported on main frame and iframes with the same
    // origin.
    return;
  }
  if (!self.delegate) {
    return;
  }

  [self.delegate formHelper:self didSubmitForm:formData inFrame:frame];
}

#pragma mark - Private methods

- (void)getPasswordForms:(std::vector<FormData>*)forms
                fromJSON:(NSString*)JSONString
                 pageURL:(const GURL&)pageURL
                   frame:(web::WebFrame*)frame {
  autofill::FieldDataManager* fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(frame);

  std::vector<FormData> formsData;
  if (!autofill::ExtractFormsData(JSONString, false, std::u16string(), pageURL,
                                  frame->GetSecurityOrigin(), *fieldDataManager,
                                  frame->GetFrameId(), &formsData)) {
    return;
  }
  *forms = std::move(formsData);
}

- (void)recordFormFillingSuccessMetrics:(bool)success {
  base::UmaHistogramBoolean("PasswordManager.FillingSuccessIOS", success);
  ukm::SourceId source_id = ukm::GetSourceIdForWebStateDocument(_webState);

  if (source_id == ukm::kInvalidSourceId || !(ukm::UkmRecorder::Get())) {
    return;
  }
  ukm::builders::PasswordManager_PasswordFillingIOS(source_id)
      .SetFillingSuccess(success)
      .Record(ukm::UkmRecorder::Get());
}

#pragma mark - Public methods

- (void)findPasswordFormsInFrame:(web::WebFrame*)frame
               completionHandler:
                   (void (^)(const std::vector<FormData>&))completionHandler {
  if (!_webState) {
    return;
  }

  std::optional<GURL> pageURL = _webState->GetLastCommittedURLIfTrusted();
  if (!pageURL) {
    return;
  }

  __weak PasswordFormHelper* weakSelf = self;
  password_manager::PasswordManagerJavaScriptFeature::GetInstance()
      ->FindPasswordFormsInFrame(
          frame, base::BindOnce(^(NSString* JSONString) {
            std::vector<FormData> forms;
            [weakSelf getPasswordForms:&forms
                              fromJSON:JSONString
                               pageURL:*pageURL
                                 frame:frame];
            completionHandler(forms);
          }));
}

- (void)fillPasswordForm:(FormRendererId)formIdentifier
                      inFrame:(web::WebFrame*)frame
        newPasswordIdentifier:(FieldRendererId)newPasswordIdentifier
    confirmPasswordIdentifier:(FieldRendererId)confirmPasswordIdentifier
            generatedPassword:(NSString*)generatedPassword
            completionHandler:(nullable void (^)(BOOL))completionHandler {
  const scoped_refptr<autofill::FieldDataManager> fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::GetRetainable(frame);

  // Send JSON over to the web view.
  password_manager::PasswordManagerJavaScriptFeature::GetInstance()
      ->FillPasswordForm(
          frame, formIdentifier, newPasswordIdentifier,
          confirmPasswordIdentifier, generatedPassword,
          base::BindOnce(
              ^(BOOL success) {
                if (success) {
                  fieldDataManager->UpdateFieldDataMap(
                      newPasswordIdentifier,
                      SysNSStringToUTF16(generatedPassword),
                      FieldPropertiesFlags::kAutofilledOnUserTrigger);
                  fieldDataManager->UpdateFieldDataMap(
                      confirmPasswordIdentifier,
                      SysNSStringToUTF16(generatedPassword),
                      FieldPropertiesFlags::kAutofilledOnUserTrigger);
                }
                if (completionHandler) {
                  completionHandler(success);
                }
              }));
}

// Handles the result from filling with fill data. Returns YES if the fill
// operation is considered as a success.
- (BOOL)handleFillResult:(const base::Value*)result
            fromFillData:(password_manager::FillData)fillData
    withFieldDataManager:(autofill::FieldDataManager*)manager
                  driver:(IOSPasswordManagerDriver*)driver {
  if (!result || !result->is_dict()) {
    return NO;
  }

  std::optional<bool> did_attempt_fill =
      result->GetDict().FindBool("didAttemptFill");
  std::optional<bool> did_fill_username =
      result->GetDict().FindBool("didFillUsername");
  std::optional<bool> did_fill_password =
      result->GetDict().FindBool("didFillPassword");

  if (!did_attempt_fill || !did_fill_username || !did_fill_password) {
    return NO;
  }
  const bool success = *did_attempt_fill;

  [self recordFormFillingSuccessMetrics:success];

  // TODO(crbug.com/347882357): Add a metric to record the reason why
  // |didFillUsername| or |didFillPassword| is false.

  // Set the fill property even if |*did_fill_password| or |*did_fill_username|
  // are false to avoid skewing the PasswordManager.FillingAssistance histogram.
  // This is the status quo with how the field data used to be updated, before
  // crrev.com/c/5494354.
  if (fillData.username_element_id && success) {
    manager->UpdateFieldDataMap(fillData.username_element_id,
                                fillData.username_value,
                                FieldPropertiesFlags::kAutofilledOnUserTrigger);

    if (*did_fill_username) {
      driver->GetPasswordManager()->UpdateStateOnUserInput(
          driver, *manager, fillData.form_id, fillData.username_element_id,
          fillData.username_value);
    }
  }
  if (fillData.password_element_id && success) {
    manager->UpdateFieldDataMap(fillData.password_element_id,
                                fillData.password_value,
                                FieldPropertiesFlags::kAutofilledOnUserTrigger);
    if (*did_fill_password) {
      driver->GetPasswordManager()->UpdateStateOnUserInput(
          driver, *manager, fillData.form_id, fillData.password_element_id,
          fillData.password_value);
    }
  }

  return success;
}

- (void)fillPasswordFormWithFillData:(password_manager::FillData)fillData
                             inFrame:(web::WebFrame*)frame
                    triggeredOnField:(FieldRendererId)fieldRendererID
                   completionHandler:
                       (nullable void (^)(BOOL))completionHandler {
  const scoped_refptr<autofill::FieldDataManager> fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::GetRetainable(frame);
  const scoped_refptr<IOSPasswordManagerDriver> driver =
      IOSPasswordManagerDriverFactory::GetRetainableDriver(_webState, frame);

  // Do not fill the username if filling was triggered on a password field and
  // the username field has user typed input.
  BOOL fillUsername =
      fieldRendererID == fillData.username_element_id ||
      !fieldDataManager->DidUserType(fillData.username_element_id);
  __weak PasswordFormHelper* weakSelf = self;
  password_manager::PasswordManagerJavaScriptFeature::GetInstance()
      ->FillPasswordForm(frame, fillData, fillUsername,
                         UTF16ToUTF8(fillData.username_value),
                         UTF16ToUTF8(fillData.password_value),
                         base::BindOnce(^(const base::Value* result) {
                           const BOOL success =
                               [weakSelf handleFillResult:result
                                             fromFillData:fillData
                                     withFieldDataManager:fieldDataManager.get()
                                                   driver:driver.get()];

                           if (completionHandler) {
                             completionHandler(success);
                           }
                         }));
}

// Finds the password form named |formName| and calls
// |completionHandler| with the populated |FormData| data structure. |found| is
// YES if the current form was found successfully, NO otherwise.
- (void)extractPasswordFormData:(FormRendererId)formIdentifier
                        inFrame:(web::WebFrame*)frame
              completionHandler:(void (^)(BOOL found, const FormData& form))
                                    completionHandler {
  DCHECK(completionHandler);

  if (!_webState) {
    return;
  }

  std::optional<GURL> pageURL = _webState->GetLastCommittedURLIfTrusted();
  if (!pageURL) {
    completionHandler(NO, FormData());
    return;
  }

  const scoped_refptr<autofill::FieldDataManager> fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::GetRetainable(frame);

  std::string frame_id = frame->GetFrameId();
  password_manager::PasswordManagerJavaScriptFeature::GetInstance()
      ->ExtractForm(
          frame, formIdentifier, base::BindOnce(^(NSString* jsonString) {
            FormData formData;
            if (!JsonStringToFormData(jsonString, &formData, *pageURL,
                                      *fieldDataManager, frame_id)) {
              completionHandler(NO, FormData());
              return;
            }

            completionHandler(YES, formData);
          }));
}

- (void)updateFieldDataOnUserInput:(autofill::FieldRendererId)field_id
                           inFrame:(web::WebFrame*)frame
                        inputValue:(NSString*)value {
  autofill::FieldDataManager* fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(frame);

  fieldDataManager->UpdateFieldDataMap(
      field_id, base::SysNSStringToUTF16(value),
      autofill::FieldPropertiesFlags::kUserTyped);
}

- (HandleSubmittedFormStatus)handleFormSubmittedMessage:
    (const web::ScriptMessage&)message {
  if (!_webState) {
    return HandleSubmittedFormStatus::kRejectedNoWebState;
  }

  if (!self.delegate) {
    return HandleSubmittedFormStatus::kRejectedNoDelegate;
  }

  std::optional<GURL> pageURL = _webState->GetLastCommittedURLIfTrusted();
  if (!pageURL) {
    return HandleSubmittedFormStatus::kRejectedNoTrustedUrl;
  }

  web::WebFrame* frame = nullptr;
  base::Value* body = message.body();

  if (!body->is_dict()) {
    // Don't handle the message if it isn't of dictionary type. The renderer
    // must provide that type of message so it can be interpreted.
    return HandleSubmittedFormStatus::kRejectedMessageBodyNotADict;
  }

  const auto& dict = body->GetDict();
  const std::string* host_frame =
      dict.FindString(password_manager::kHostFrameKey);
  if (host_frame) {
    password_manager::PasswordManagerJavaScriptFeature* feature =
        password_manager::PasswordManagerJavaScriptFeature::GetInstance();
    frame =
        feature->GetWebFramesManager(_webState)->GetFrameWithId(*host_frame);
  }
  if (!frame) {
    return HandleSubmittedFormStatus::kRejectedNoFrameMatchingId;
  }

  autofill::FieldDataManager* fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(frame);

  FormData form;
  if (!autofill::ExtractFormData(dict, false, std::u16string(), *pageURL,
                                 pageURL->DeprecatedGetOriginAsURL(),
                                 *fieldDataManager, *host_frame, &form)) {
    return HandleSubmittedFormStatus::kRejectedCantExtractFormData;
  }

  [self.delegate formHelper:self didSubmitForm:form inFrame:frame];

  return HandleSubmittedFormStatus::kHandled;
}

@end
