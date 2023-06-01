// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_form_helper.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#include "components/password_manager/ios/password_manager_ios_util.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "components/password_manager/ios/password_manager_tab_helper.h"
#include "components/ukm/ios/ukm_url_recorder.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#include "services/metrics/public/cpp/ukm_builders.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FieldPropertiesFlags;
using autofill::FormData;
using autofill::FormRendererId;
using autofill::FieldRendererId;
using autofill::PasswordFormFillData;
using base::SysNSStringToUTF16;
using base::UTF16ToUTF8;
using password_manager::FillData;
using password_manager::GetPageURLAndCheckTrustLevel;
using password_manager::JsonStringToFormData;

namespace password_manager {
bool GetPageURLAndCheckTrustLevel(web::WebState* web_state,
                                  GURL* __nullable page_url) {
  absl::optional<GURL> last_committed_url =
      web_state->GetLastCommittedURLIfTrusted();
  if (last_committed_url) {
    if (page_url) {
      *page_url = std::move(*last_committed_url);
    }
    return true;
  }
  return false;
}

// The frame id associated with the frame which sent to form message.
const char kFrameIdKey[] = "frame_id";

}  // namespace password_manager

@interface PasswordFormHelper ()

// Parses the |jsonString| which contatins the password forms found on a web
// page to populate the |forms| vector.
- (void)getPasswordForms:(std::vector<FormData>*)forms
                fromJSON:(NSString*)jsonString
                 pageURL:(const GURL&)pageURL
             frameOrigin:(const GURL&)frameOrigin;

// Records both UMA & UKM metrics.
- (void)recordFormFillingSuccessMetrics:(bool)success;

@end

@implementation PasswordFormHelper {
  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* _webState;

  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe form activity in |_webState|.
  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;
}

#pragma mark - Properties

@synthesize fieldDataManager = _fieldDataManager;

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

    UniqueIDDataTabHelper* uniqueIDDataTabHelper =
        UniqueIDDataTabHelper::FromWebState(_webState);
    _fieldDataManager = uniqueIDDataTabHelper->GetFieldDataManager();

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
    didSubmitDocumentWithFormNamed:(const std::string&)formName
                          withData:(const std::string&)formData
                    hasUserGesture:(BOOL)hasUserGesture
                           inFrame:(web::WebFrame*)frame {
  DCHECK_EQ(_webState, webState);
  GURL pageURL = webState->GetLastCommittedURL();
  if (pageURL.DeprecatedGetOriginAsURL() != frame->GetSecurityOrigin()) {
    // Passwords is only supported on main frame and iframes with the same
    // origin.
    return;
  }
  if (!self.delegate || formData.empty()) {
    return;
  }
  std::vector<FormData> forms;
  NSString* nsFormData = [NSString stringWithUTF8String:formData.c_str()];
  autofill::ExtractFormsData(nsFormData, false, std::u16string(), pageURL,
                             pageURL.DeprecatedGetOriginAsURL(),
                             *self.fieldDataManager, &forms);
  if (forms.size() != 1) {
    return;
  }

  [self.delegate formHelper:self didSubmitForm:forms[0] inFrame:frame];
}

#pragma mark - Private methods

- (void)getPasswordForms:(std::vector<FormData>*)forms
                fromJSON:(NSString*)JSONString
                 pageURL:(const GURL&)pageURL
             frameOrigin:(const GURL&)frameOrigin {
  std::vector<FormData> formsData;
  if (!autofill::ExtractFormsData(JSONString, false, std::u16string(), pageURL,
                                  frameOrigin, *self.fieldDataManager,
                                  &formsData)) {
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
               completionHandler:(void (^)(const std::vector<FormData>&,
                                           uint32_t))completionHandler {
  if (!_webState) {
    return;
  }

  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(_webState, &pageURL)) {
    return;
  }

  __weak PasswordFormHelper* weakSelf = self;
  password_manager::PasswordManagerJavaScriptFeature::GetInstance()
      ->FindPasswordFormsInFrame(
          frame, base::BindOnce(^(NSString* JSONString) {
            std::vector<FormData> forms;
            [weakSelf getPasswordForms:&forms
                              fromJSON:JSONString
                               pageURL:pageURL
                           frameOrigin:frame->GetSecurityOrigin()];
            // Find the maximum extracted value.
            uint32_t maxID = 0;
            for (const auto& form : forms) {
              if (form.unique_renderer_id) {
                maxID = std::max(maxID, form.unique_renderer_id.value());
              }
              for (const auto& field : form.fields) {
                if (field.unique_renderer_id) {
                  maxID = std::max(maxID, field.unique_renderer_id.value());
                }
              }
            }
            completionHandler(forms, maxID);
          }));
}

- (void)fillPasswordForm:(FormRendererId)formIdentifier
                      inFrame:(web::WebFrame*)frame
        newPasswordIdentifier:(FieldRendererId)newPasswordIdentifier
    confirmPasswordIdentifier:(FieldRendererId)confirmPasswordIdentifier
            generatedPassword:(NSString*)generatedPassword
            completionHandler:(nullable void (^)(BOOL))completionHandler {
  // Send JSON over to the web view.
  __weak PasswordFormHelper* weakSelf = self;
  password_manager::PasswordManagerJavaScriptFeature::GetInstance()
      ->FillPasswordForm(
          frame, formIdentifier, newPasswordIdentifier,
          confirmPasswordIdentifier, generatedPassword,
          base::BindOnce(
              ^(BOOL success) {
                if (success) {
                  weakSelf.fieldDataManager->UpdateFieldDataMap(
                      newPasswordIdentifier,
                      SysNSStringToUTF16(generatedPassword),
                      FieldPropertiesFlags::kAutofilledOnUserTrigger);
                  weakSelf.fieldDataManager->UpdateFieldDataMap(
                      confirmPasswordIdentifier,
                      SysNSStringToUTF16(generatedPassword),
                      FieldPropertiesFlags::kAutofilledOnUserTrigger);
                }
                if (completionHandler) {
                  completionHandler(success);
                }
              }));
}

- (void)fillPasswordFormWithFillData:(const password_manager::FillData&)fillData
                             inFrame:(web::WebFrame*)frame
                    triggeredOnField:(FieldRendererId)uniqueFieldID
                   completionHandler:
                       (nullable void (^)(BOOL))completionHandler {
  // Necessary copy so the values can be used inside a block.
  FieldRendererId usernameID = fillData.username_element_id;
  FieldRendererId passwordID = fillData.password_element_id;
  std::u16string usernameValue = fillData.username_value;
  std::u16string passwordValue = fillData.password_value;

  // Do not fill the username if filling was triggered on a password field and
  // the username field has user typed input.
  BOOL fillUsername = uniqueFieldID == usernameID ||
                      !_fieldDataManager->DidUserType(usernameID);
  __weak PasswordFormHelper* weakSelf = self;
  password_manager::PasswordManagerJavaScriptFeature::GetInstance()
      ->FillPasswordForm(
          frame, fillData, fillUsername, UTF16ToUTF8(usernameValue),
          UTF16ToUTF8(passwordValue), base::BindOnce(^(BOOL success) {
            PasswordFormHelper* strongSelf = weakSelf;
            if (!strongSelf) {
              return;
            }
            [strongSelf recordFormFillingSuccessMetrics:success];
            if (success) {
              strongSelf.fieldDataManager->UpdateFieldDataMap(
                  usernameID, usernameValue,
                  FieldPropertiesFlags::kAutofilledOnUserTrigger);
              strongSelf.fieldDataManager->UpdateFieldDataMap(
                  passwordID, passwordValue,
                  FieldPropertiesFlags::kAutofilledOnUserTrigger);
            }
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

  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(_webState, &pageURL)) {
    completionHandler(NO, FormData());
    return;
  }

  scoped_refptr<autofill::FieldDataManager> fieldDataManager =
      _fieldDataManager;
  password_manager::PasswordManagerJavaScriptFeature::GetInstance()
      ->ExtractForm(
          frame, formIdentifier, base::BindOnce(^(NSString* jsonString) {
            FormData formData;
            if (!JsonStringToFormData(jsonString, &formData, pageURL,
                                      *fieldDataManager)) {
              completionHandler(NO, FormData());
              return;
            }

            completionHandler(YES, formData);
          }));
}

- (void)setUpForUniqueIDsWithInitialState:(uint32_t)nextAvailableID
                                  inFrame:(web::WebFrame*)frame {
  autofill::FormUtilJavaScriptFeature::GetInstance()
      ->SetUpForUniqueIDsWithInitialState(frame, nextAvailableID);
}

- (void)updateFieldDataOnUserInput:(autofill::FieldRendererId)field_id
                        inputValue:(NSString*)value {
  self.fieldDataManager->UpdateFieldDataMap(
      field_id, base::SysNSStringToUTF16(value),
      autofill::FieldPropertiesFlags::kUserTyped);
}

- (void)handleFormSubmittedMessage:(const web::ScriptMessage&)message {
  web::WebFrame* frame = nullptr;
  const auto& dict = message.body()->GetDict();
  const std::string* frame_id = dict.FindString(password_manager::kFrameIdKey);
  if (frame_id) {
    password_manager::PasswordManagerJavaScriptFeature* feature =
        password_manager::PasswordManagerJavaScriptFeature::GetInstance();
    frame = feature->GetWebFramesManager(_webState)->GetFrameWithId(*frame_id);
  }
  if (!frame) {
    return;
  }

  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(_webState, &pageURL)) {
    return;
  }

  FormData form;
  if (!autofill::ExtractFormData(dict, false, std::u16string(), pageURL,
                                 pageURL.DeprecatedGetOriginAsURL(),
                                 *self.fieldDataManager, &form)) {
    return;
  }

  if (_webState && self.delegate) {
    [self.delegate formHelper:self didSubmitForm:form inFrame:frame];
  }
}

@end
