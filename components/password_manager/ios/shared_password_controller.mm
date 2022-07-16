// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/shared_password_controller.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/form_suggestion_provider_query.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#include "components/password_manager/ios/password_manager_ios_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/web/common/url_scheme_util.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#include "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FieldDataManager;
using autofill::FieldRendererId;
using autofill::FormActivityObserverBridge;
using autofill::FormData;
using autofill::FormRendererId;
using autofill::PasswordFormGenerationData;
using autofill::password_generation::LogPasswordGenerationEvent;
using autofill::password_generation::PasswordGenerationType;
using base::SysNSStringToUTF16;
using base::SysUTF16ToNSString;
using base::SysUTF8ToNSString;
using l10n_util::GetNSString;
using l10n_util::GetNSStringF;
using password_manager::AccountSelectFillData;
using password_manager::FillData;
using password_manager::GetPageURLAndCheckTrustLevel;
using password_manager::JsonStringToFormData;
using password_manager::metrics_util::LogPasswordDropdownShown;
using password_manager::metrics_util::PasswordDropdownState;
using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordGenerationFrameHelper;
using password_manager::PasswordManagerClient;
using password_manager::PasswordManagerDriver;
using password_manager::PasswordManagerInterface;

namespace {

// Password is considered not generated when user edits it below 4 characters.
constexpr int kMinimumLengthForEditedPassword = 4;

// The string ' •••' appended to the username in the suggestion.
NSString* const kSuggestionSuffix = @" ••••••••";
}  // namespace

@interface SharedPasswordController ()

// Helper contains common password suggestion logic.
@property(nonatomic, readonly) PasswordSuggestionHelper* suggestionHelper;

// Tracks if current password is generated.
@property(nonatomic, assign) BOOL isPasswordGenerated;

// Tracks field when current password was generated.
@property(nonatomic) FieldRendererId passwordGeneratedIdentifier;

// Tracks current potential generated password until accepted or rejected.
@property(nonatomic, copy) NSString* generatedPotentialPassword;

- (BOOL)isIncognito;

@end

@implementation SharedPasswordController {
  PasswordManagerInterface* _passwordManager;
  std::unique_ptr<PasswordGenerationFrameHelper> _passwordGenerationHelper;

  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* _webState;

  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe form activity in |_webState|.
  std::unique_ptr<FormActivityObserverBridge> _formActivityObserverBridge;

  // Form data for password generation on this page.
  std::map<FormRendererId, PasswordFormGenerationData> _formGenerationData;

  // Identifier of the field that was last typed into.
  FieldRendererId _lastTypedfieldIdentifier;

  // The value that was last typed by the user.
  NSString* _lastTypedValue;

  // Identifier of the last focused form.
  FormRendererId _lastFocusedFormIdentifier;

  // Identifier of the last focused field.
  FieldRendererId _lastFocusedFieldIdentifier;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                         manager:(password_manager::PasswordManagerInterface*)
                                     passwordManager
                      formHelper:(PasswordFormHelper*)formHelper
                suggestionHelper:(PasswordSuggestionHelper*)suggestionHelper {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webState = webState;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    _formActivityObserverBridge =
        std::make_unique<FormActivityObserverBridge>(_webState, self);
    _formHelper = formHelper;
    _formHelper.delegate = self;
    _suggestionHelper = suggestionHelper;
    _suggestionHelper.delegate = self;
    _passwordManager = passwordManager;
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
}

- (BOOL)isIncognito {
  DCHECK(_delegate.passwordManagerClient);
  return _delegate.passwordManagerClient->IsIncognito();
}

#pragma mark - PasswordGenerationProvider

- (void)triggerPasswordGeneration {
  if (!_lastFocusedFieldIdentifier) {
    return;
  }
  LogPasswordGenerationEvent(
      autofill::password_generation::PASSWORD_GENERATION_CONTEXT_MENU_PRESSED);
  [self generatePasswordForFormId:_lastFocusedFormIdentifier
                  fieldIdentifier:_lastFocusedFieldIdentifier
              isManuallyTriggered:YES];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  if (!navigation->HasCommitted() || navigation->IsSameDocument()) {
    return;
  }

  if (!GetPageURLAndCheckTrustLevel(webState, nullptr)) {
    return;
  }

  // Clear per-page state.
  [self.suggestionHelper resetForNewPage];

  auto fieldDataManager =
      UniqueIDDataTabHelper::FromWebState(_webState)->GetFieldDataManager();
  _passwordManager->PropagateFieldDataManagerInfo(
      *fieldDataManager, _delegate.passwordManagerDriver);
  // On non-iOS platforms navigations initiated by link click are excluded from
  // navigations which might be form submssions. On iOS there is no easy way to
  // check that the navigation is link initiated, so it is skipped. It should
  // not be so important since it is unlikely that the user clicks on a link
  // after filling password form w/o submitting it.
  _passwordManager->DidNavigateMainFrame(
      /*form_may_be_submitted=*/navigation->IsRendererInitiated());
  fieldDataManager->ClearData();
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);

  // Retrieve the identity of the page. In case the page might be malicous,
  // returns early.
  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(webState, &pageURL)) {
    return;
  }

  if (!web::UrlHasWebScheme(pageURL)) {
    return;
  }

  if (!webState->ContentIsHTML()) {
    // If the current page is not HTML, it does not contain any HTML forms.
    UniqueIDDataTabHelper* uniqueIDDataTabHelper =
        UniqueIDDataTabHelper::FromWebState(_webState);
    uint32_t maxUniqueID = uniqueIDDataTabHelper->GetNextAvailableRendererID();
    [self didFinishPasswordFormExtraction:std::vector<FormData>()
                          withMaxUniqueID:maxUniqueID];
  }
}

- (void)webState:(web::WebState*)webState
    frameDidBecomeAvailable:(web::WebFrame*)web_frame {
  DCHECK_EQ(_webState, webState);
  DCHECK(web_frame);
  if (!web_frame->CanCallJavaScriptFunction()) {
    return;
  }
  UniqueIDDataTabHelper* uniqueIDDataTabHelper =
      UniqueIDDataTabHelper::FromWebState(_webState);
  uint32_t nextAvailableRendererID =
      uniqueIDDataTabHelper->GetNextAvailableRendererID();
  [self.formHelper setUpForUniqueIDsWithInitialState:nextAvailableRendererID
                                             inFrame:web_frame];
  // Form parsing is run via the main frame for all same origin iframes.
  if (web_frame->IsMainFrame() && webState->ContentIsHTML()) {
    [self findPasswordFormsAndSendThemToPasswordStore];
  }
}

// Track detaching iframes.
- (void)webState:(web::WebState*)webState
    frameWillBecomeUnavailable:(web::WebFrame*)web_frame {
  // No need to try to detect submissions when the webState is being destroyed.
  if (webState->IsBeingDestroyed()) {
    return;
  }
  if (web_frame->IsMainFrame() || !web_frame->CanCallJavaScriptFunction()) {
    return;
  }
  _passwordManager->OnIframeDetach(web_frame->GetFrameId(),
                                   _delegate.passwordManagerDriver,
                                   *self.formHelper.fieldDataManager);
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _formActivityObserverBridge.reset();
    _webState = nullptr;
  }
  _formGenerationData.clear();
  _isPasswordGenerated = NO;
  _lastTypedfieldIdentifier = FieldRendererId();
  _lastTypedValue = nil;
  _lastFocusedFormIdentifier = FormRendererId();
  _lastFocusedFieldIdentifier = FieldRendererId();
  _passwordManager = nullptr;
}

#pragma mark - FormSuggestionProvider

- (void)checkIfSuggestionsAvailableForForm:
            (FormSuggestionProviderQuery*)formQuery
                               isMainFrame:(BOOL)isMainFrame
                            hasUserGesture:(BOOL)hasUserGesture
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  if (!GetPageURLAndCheckTrustLevel(webState, nullptr)) {
    return;
  }
  [self.suggestionHelper
      checkIfSuggestionsAvailableForForm:formQuery
                             isMainFrame:isMainFrame
                                webState:webState
                       completionHandler:^(BOOL suggestionsAvailable) {
                         // Always display "Show All..." for password fields.
                         completion([formQuery isOnPasswordField] ||
                                    suggestionsAvailable);
                       }];

  if (self.isPasswordGenerated &&
      ([formQuery.type isEqual:@"input"] ||
       [formQuery.type isEqual:@"keyup"]) &&
      formQuery.uniqueFieldID == self.passwordGeneratedIdentifier) {
    // On other platforms, when the user clicks on generation field, we show
    // password in clear text. And the user has the possibility to edit it. On
    // iOS, it's harder to do (it's probably bad idea to change field type from
    // password to text). The decision was to give everything to the automatic
    // flow and avoid the manual flow, for a cleaner and simpler UI.
    if (formQuery.typedValue.length < kMinimumLengthForEditedPassword) {
      self.isPasswordGenerated = NO;
      LogPasswordGenerationEvent(
          autofill::password_generation::PASSWORD_DELETED);
      self.passwordGeneratedIdentifier = FieldRendererId();
      _passwordManager->OnPasswordNoLongerGenerated(
          _delegate.passwordManagerDriver);
    } else {
      // Inject updated value to possibly update confirmation field.
      [self injectGeneratedPasswordForFormId:formQuery.uniqueFormID
                           generatedPassword:formQuery.typedValue
                           completionHandler:nil];
    }
  }

  if (formQuery.uniqueFieldID != _lastTypedfieldIdentifier ||
      ![formQuery.typedValue isEqual:_lastTypedValue]) {
    // This method is called multiple times for the same user keystroke. Inform
    // only once the keystroke.
    _lastTypedfieldIdentifier = formQuery.uniqueFieldID;
    _lastTypedValue = formQuery.typedValue;

    if ([formQuery.type isEqual:@"input"] ||
        [formQuery.type isEqual:@"keyup"]) {
      [self.formHelper updateFieldDataOnUserInput:formQuery.uniqueFieldID
                                       inputValue:formQuery.typedValue];

      _passwordManager->UpdateStateOnUserInput(
          _delegate.passwordManagerDriver, formQuery.uniqueFormID,
          formQuery.uniqueFieldID, SysNSStringToUTF16(formQuery.typedValue));
    }
  }
}

- (void)retrieveSuggestionsForForm:(FormSuggestionProviderQuery*)formQuery
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  if (!GetPageURLAndCheckTrustLevel(webState, nullptr)) {
    return;
  }
  NSArray<FormSuggestion*>* rawSuggestions = [self.suggestionHelper
      retrieveSuggestionsWithFormID:formQuery.uniqueFormID
                    fieldIdentifier:formQuery.uniqueFieldID
                          fieldType:formQuery.fieldType];

  NSMutableArray<FormSuggestion*>* suggestions = [NSMutableArray array];
  bool isPasswordField = [formQuery isOnPasswordField];
  for (FormSuggestion* rawSuggestion in rawSuggestions) {
    // 1) If this is a focus event or the field is empty show all suggestions.
    // Otherwise:
    // 2) If this is a username field then show only credentials with matching
    // prefixes.
    // 3) If this is a password field then show suggestions only if
    // the field is empty.
    if (![formQuery hasFocusType] && formQuery.typedValue.length > 0 &&
        (isPasswordField ||
         ![rawSuggestion.value hasPrefix:formQuery.typedValue])) {
      continue;
    }
    DCHECK(self.delegate.passwordManagerClient);
    NSString* value =
        [rawSuggestion.value stringByAppendingString:kSuggestionSuffix];
    FormSuggestion* suggestion =
        [FormSuggestion suggestionWithValue:value
                         displayDescription:rawSuggestion.displayDescription
                                       icon:nil
                                 identifier:0
                             requiresReauth:YES];
    [suggestions addObject:suggestion];
  }
  absl::optional<PasswordDropdownState> suggestionState;
  if (suggestions.count) {
    suggestionState = PasswordDropdownState::kStandard;
  }

  if ([self canGeneratePasswordForForm:formQuery.uniqueFormID
                       fieldIdentifier:formQuery.uniqueFieldID
                             fieldType:formQuery.fieldType]) {
    // Add "Suggest Password...".
    NSString* suggestPassword = GetNSString(IDS_IOS_SUGGEST_PASSWORD);
    FormSuggestion* suggestion = [FormSuggestion
        suggestionWithValue:suggestPassword
         displayDescription:nil
                       icon:nil
                 identifier:autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY
             requiresReauth:NO];

    [suggestions addObject:suggestion];
    suggestionState = PasswordDropdownState::kStandardGenerate;
  }

  if (suggestionState) {
    LogPasswordDropdownShown(*suggestionState, [self isIncognito]);
  }

  completion([suggestions copy], self);
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                       form:(NSString*)formName
               uniqueFormID:(FormRendererId)uniqueFormID
            fieldIdentifier:(NSString*)fieldIdentifier
              uniqueFieldID:(FieldRendererId)uniqueFieldID
                    frameID:(NSString*)frameID
          completionHandler:(SuggestionHandledCompletion)completion {
  switch (suggestion.identifier) {
    case autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY: {
      completion();
      password_manager::metrics_util::LogPasswordDropdownItemSelected(
          password_manager::metrics_util::PasswordDropdownSelectedOption::
              kShowAll,
          [self isIncognito]);
      return;
    }
    case autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY: {
      // Don't call completion because current suggestion state should remain
      // whether user injects a generated password or cancels.
      [self generatePasswordForFormId:uniqueFormID
                      fieldIdentifier:uniqueFieldID
                  isManuallyTriggered:NO];
      password_manager::metrics_util::LogPasswordDropdownItemSelected(
          password_manager::metrics_util::PasswordDropdownSelectedOption::
              kGenerate,
          [self isIncognito]);
      return;
    }
    default: {
      password_manager::metrics_util::LogPasswordDropdownItemSelected(
          password_manager::metrics_util::PasswordDropdownSelectedOption::
              kPassword,
          [self isIncognito]);
      DCHECK([suggestion.value hasSuffix:kSuggestionSuffix]);
      NSString* username = [suggestion.value
          substringToIndex:suggestion.value.length - kSuggestionSuffix.length];
      std::unique_ptr<password_manager::FillData> fillData =
          [self.suggestionHelper passwordFillDataForUsername:username];

      if (!fillData) {
        completion();
        return;
      }

      [self.formHelper fillPasswordFormWithFillData:*fillData
                                   triggeredOnField:uniqueFieldID
                                  completionHandler:^(BOOL success) {
                                    completion();
                                  }];
      break;
    }
  }

  [_delegate sharedPasswordController:self didAcceptSuggestion:suggestion];
}

- (SuggestionProviderType)type {
  return SuggestionProviderTypePassword;
}

#pragma mark - PasswordManagerDriverDelegate

- (const GURL&)lastCommittedURL {
  return _webState ? _webState->GetLastCommittedURL() : GURL::EmptyGURL();
}

- (void)fillPasswordForm:(const autofill::PasswordFormFillData&)formData
       completionHandler:(void (^)(BOOL))completionHandler {
  [self.suggestionHelper processWithPasswordFormFillData:formData];
  [self.formHelper fillPasswordForm:formData
                  completionHandler:completionHandler];
}

- (void)onNoSavedCredentials {
  [self.suggestionHelper processWithNoSavedCredentials];
}

- (PasswordGenerationFrameHelper*)passwordGenerationHelper {
  if (![self isIncognito]) {
    _passwordGenerationHelper.reset(new PasswordGenerationFrameHelper(
        _delegate.passwordManagerClient, _delegate.passwordManagerDriver));
  }
  return _passwordGenerationHelper.get();
}

- (void)formEligibleForGenerationFound:(const PasswordFormGenerationData&)form {
  _formGenerationData[form.form_renderer_id] = form;
}

#pragma mark - PasswordFormHelperDelegate

- (void)formHelper:(PasswordFormHelper*)formHelper
     didSubmitForm:(const FormData&)form
       inMainFrame:(BOOL)inMainFrame {
  if (inMainFrame) {
    _passwordManager->OnPasswordFormSubmitted(_delegate.passwordManagerDriver,
                                              form);
  } else {
    // Show a save prompt immediately because for iframes it is very hard to
    // figure out correctness of password forms submission.
    _passwordManager->OnSubframeFormSubmission(_delegate.passwordManagerDriver,
                                               form);
  }
}

#pragma mark - PasswordSuggestionHelperDelegate

- (void)suggestionHelperShouldTriggerFormExtraction:
    (PasswordSuggestionHelper*)suggestionHelper {
  [self findPasswordFormsAndSendThemToPasswordStore];
}

#pragma mark - Private methods

- (void)didFinishPasswordFormExtraction:(const std::vector<FormData>&)forms
                        withMaxUniqueID:(uint32_t)maxID {
  // Do nothing if |self| has been detached.
  if (!_passwordManager) {
    return;
  }

  if (!forms.empty()) {
    [self.suggestionHelper updateStateOnPasswordFormExtracted];
    UniqueIDDataTabHelper* uniqueIDDataTabHelper =
        UniqueIDDataTabHelper::FromWebState(_webState);
    // Update NextAvailableRendererId if a bigger value was extracted.
    if (uniqueIDDataTabHelper->GetNextAvailableRendererID() < maxID)
      uniqueIDDataTabHelper->SetNextAvailableRendererID(++maxID);

    // Invoke the password manager callback to autofill password forms
    // on the loaded page.
    _passwordManager->OnPasswordFormsParsed(_delegate.passwordManagerDriver,
                                            forms);
  } else {
    [self onNoSavedCredentials];
  }
  // Invoke the password manager callback to check if password was
  // accepted or rejected. If accepted, infobar is presented. If
  // rejected, the provisionally saved password is deleted. On Chrome
  // w/ a renderer, it is the renderer who calls OnPasswordFormsParsed()
  // and OnPasswordFormsRendered(). Bling has to improvised a bit on the
  // ordering of these two calls.
  _passwordManager->OnPasswordFormsRendered(_delegate.passwordManagerDriver,
                                            forms, true);
}

- (void)findPasswordFormsAndSendThemToPasswordStore {
  // Read all password forms from the page and send them to the password
  // manager.
  __weak SharedPasswordController* weakSelf = self;
  [self.formHelper findPasswordFormsWithCompletionHandler:^(
                       const std::vector<FormData>& forms, uint32_t maxID) {
    [weakSelf didFinishPasswordFormExtraction:forms withMaxUniqueID:maxID];
  }];
}

- (BOOL)canGeneratePasswordForForm:(FormRendererId)formIdentifier
                   fieldIdentifier:(FieldRendererId)fieldIdentifier
                         fieldType:(NSString*)fieldType {
  if ([self isIncognito] || !self.passwordGenerationHelper->IsGenerationEnabled(
                                /*log_debug_data*/ true)) {
    return NO;
  }
  if (![fieldType isEqual:kPasswordFieldType])
    return NO;
  const PasswordFormGenerationData* generationData =
      [self formForGenerationFromFormID:formIdentifier];
  if (!generationData) {
    return NO;
  }

  FieldRendererId newPasswordIdentifier =
      generationData->new_password_renderer_id;
  if (fieldIdentifier == newPasswordIdentifier) {
    return YES;
  }

  // Don't show password generation if the field is 'confirm password'.
  return NO;
}

- (const PasswordFormGenerationData*)formForGenerationFromFormID:
    (FormRendererId)formIdentifier {
  if (_formGenerationData.find(formIdentifier) != _formGenerationData.end()) {
    return &_formGenerationData[formIdentifier];
  }
  return nullptr;
}

- (void)generatePasswordForFormId:(FormRendererId)formIdentifier
                  fieldIdentifier:(FieldRendererId)fieldIdentifier
              isManuallyTriggered:(BOOL)isManuallyTriggered {
  const autofill::PasswordFormGenerationData* generationData =
      [self formForGenerationFromFormID:formIdentifier];
  if (!isManuallyTriggered && !generationData) {
    return;
  }

  BOOL shouldUpdateGenerationData =
      !generationData ||
      generationData->new_password_renderer_id != fieldIdentifier;
  if (isManuallyTriggered && shouldUpdateGenerationData) {
    PasswordFormGenerationData generationData = {
        .form_renderer_id = formIdentifier,
        .new_password_renderer_id = fieldIdentifier,
    };
    [self formEligibleForGenerationFound:generationData];
  }

  // TODO(crbug.com/886583): pass correct |max_length|.
  std::u16string generatedPassword =
      self.passwordGenerationHelper->GeneratePassword(
          [self lastCommittedURL], autofill::FormSignature(0),
          autofill::FieldSignature(0), /*max_length=*/0);

  self.generatedPotentialPassword = SysUTF16ToNSString(generatedPassword);

  __weak SharedPasswordController* weakSelf = self;
  auto completionHandler = ^{
    weakSelf.generatedPotentialPassword = nil;
  };

  [_delegate sharedPasswordController:self
       showGeneratedPotentialPassword:self.generatedPotentialPassword
                      decisionHandler:^(BOOL accept) {
                        if (accept) {
                          LogPasswordGenerationEvent(
                              autofill::password_generation::PASSWORD_ACCEPTED);
                          [weakSelf
                              injectGeneratedPasswordForFormId:formIdentifier
                                             generatedPassword:
                                                 weakSelf
                                                     .generatedPotentialPassword
                                             completionHandler:
                                                 completionHandler];
                        } else {
                          completionHandler();
                        }
                      }];

  _passwordManager->SetGenerationElementAndTypeForForm(
      _delegate.passwordManagerDriver, formIdentifier, fieldIdentifier,
      isManuallyTriggered ? PasswordGenerationType::kManual
                          : PasswordGenerationType::kAutomatic);
}

- (void)injectGeneratedPasswordForFormId:(FormRendererId)formIdentifier
                       generatedPassword:(NSString*)generatedPassword
                       completionHandler:(void (^)())completionHandler {
  const autofill::PasswordFormGenerationData* generationData =
      [self formForGenerationFromFormID:formIdentifier];
  if (!generationData) {
    return;
  }
  FieldRendererId newPasswordUniqueId =
      generationData->new_password_renderer_id;
  FieldRendererId confirmPasswordUniqueId =
      generationData->confirmation_password_renderer_id;

  __weak SharedPasswordController* weakSelf = self;
  auto generatedPasswordInjected = ^(BOOL success) {
    if (success) {
      [weakSelf onFilledPasswordForm:formIdentifier
               withGeneratedPassword:generatedPassword
                    passwordUniqueId:newPasswordUniqueId];
    }
    if (completionHandler) {
      completionHandler();
    }
  };

  [self.formHelper fillPasswordForm:formIdentifier
              newPasswordIdentifier:newPasswordUniqueId
          confirmPasswordIdentifier:confirmPasswordUniqueId
                  generatedPassword:generatedPassword
                  completionHandler:generatedPasswordInjected];
}

- (void)onFilledPasswordForm:(FormRendererId)formIdentifier
       withGeneratedPassword:(NSString*)generatedPassword
            passwordUniqueId:(FieldRendererId)newPasswordUniqueId {
  __weak SharedPasswordController* weakSelf = self;
  auto passwordPresaved = ^(BOOL found, const autofill::FormData& form) {
    // If the form isn't found, it disappeared between the call to
    // [self.formHelper fillPasswordForm:newPasswordIdentifier:...]
    // and here. There isn't much that can be done.
    if (!found)
      return;

    [weakSelf presaveGeneratedPassword:generatedPassword
                      passwordUniqueId:newPasswordUniqueId
                              formData:form];
  };

  [self.formHelper extractPasswordFormData:formIdentifier
                         completionHandler:passwordPresaved];
  self.isPasswordGenerated = YES;
  self.passwordGeneratedIdentifier = newPasswordUniqueId;
}

- (void)presaveGeneratedPassword:(NSString*)generatedPassword
                passwordUniqueId:(FieldRendererId)newPasswordUniqueId
                        formData:(const autofill::FormData&)formData {
  if (!_passwordManager)
    return;

  _passwordManager->PresaveGeneratedPassword(
      _delegate.passwordManagerDriver, formData,
      SysNSStringToUTF16(generatedPassword), newPasswordUniqueId);
}

// Checks that all fields with |fieldIds| have user input recorded by
// the FieldDataManager.
- (BOOL)allFieldsContainUserInput:
    (const std::vector<FieldRendererId>&)fieldIds {
  for (auto fieldId : fieldIds) {
    if (!self.formHelper.fieldDataManager->HasFieldData(fieldId)) {
      return NO;
    }
  }
  return YES;
}

#pragma mark - FormActivityObserver

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  DCHECK_EQ(_webState, webState);

  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(webState, &pageURL) || !frame ||
      !frame->CanCallJavaScriptFunction() || params.input_missing) {
    _lastFocusedFormIdentifier = FormRendererId();
    _lastFocusedFieldIdentifier = FieldRendererId();
    return;
  }

  if (params.type == "focus") {
    _lastFocusedFormIdentifier = params.unique_form_id;
    _lastFocusedFieldIdentifier = params.unique_field_id;
  }

  // If there's a change in password forms on a page, they should be parsed
  // again.
  if (params.type == "form_changed") {
    [self findPasswordFormsAndSendThemToPasswordStore];
  }

  // If the form was cleared PasswordManager should be informed to decide
  // whether it's a change password form that was submitted.
  if (params.type == "password_form_cleared") {
    FormData formData;
    if (!JsonStringToFormData(SysUTF8ToNSString(params.value), &formData,
                              pageURL)) {
      return;
    }

    _passwordManager->OnPasswordFormCleared(_delegate.passwordManagerDriver,
                                            formData);
  }
}

// If the form was removed, PasswordManager should be informed to decide
// whether the form was submitted.
- (void)webState:(web::WebState*)webState
    didRegisterFormRemoval:(const autofill::FormRemovalParams&)params
                   inFrame:(web::WebFrame*)frame {
  if (!params.unique_form_id) {
    // If formless password fields were removed, check that all of them had
    // user input.
    if (![self allFieldsContainUserInput:params.removed_unowned_fields]) {
      return;
    }
  }
  _passwordManager->OnPasswordFormRemoved(_delegate.passwordManagerDriver,
                                          *self.formHelper.fieldDataManager,
                                          params.unique_form_id);
}

@end
