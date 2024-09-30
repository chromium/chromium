// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/shared_password_controller.h"

#import <stddef.h>

#import <algorithm>
#import <map>
#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/ranges/algorithm.h"
#import "base/scoped_multi_source_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "components/autofill/core/browser/filling_product.h"
#import "components/autofill/core/browser/form_structure.h"
#import "components/autofill/core/browser/ui/suggestion_type.h"
#import "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/field_data_manager.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/password_form_fill_data.h"
#import "components/autofill/core/common/password_form_generation_data.h"
#import "components/autofill/core/common/password_generation_util.h"
#import "components/autofill/core/common/signatures.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_manager_observer_bridge.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/form_suggestion_provider_query.h"
#import "components/autofill/ios/browser/password_autofill_agent.h"
#import "components/autofill/ios/common/field_data_manager_factory_ios.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/password_manager/core/browser/password_bubble_experiment.h"
#import "components/password_manager/core/browser/password_feature_manager.h"
#import "components/password_manager/core/browser/password_generation_frame_helper.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/ios/account_select_fill_data.h"
#import "components/password_manager/ios/constants.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/password_manager/ios/password_manager_ios_util.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "components/password_manager/ios/shared_password_controller+private.h"
#import "components/strings/grit/components_strings.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/js_messaging/web_frames_manager_observer_bridge.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using autofill::AutofillManager;
using autofill::AutofillManagerObserverBridge;
using autofill::FieldDataManager;
using autofill::FieldRendererId;
using autofill::FormActivityObserverBridge;
using autofill::FormData;
using autofill::FormGlobalId;
using autofill::FormRendererId;
using autofill::PasswordFormGenerationData;
using autofill::password_generation::LogPasswordGenerationEvent;
using autofill::password_generation::PasswordGenerationType;
using base::SysNSStringToUTF16;
using base::SysNSStringToUTF8;
using base::SysUTF16ToNSString;
using base::SysUTF8ToNSString;
using l10n_util::GetNSString;
using l10n_util::GetNSStringF;
using password_manager::AccountSelectFillData;
using password_manager::FillData;
using password_manager::JsonStringToFormData;
using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordGenerationFrameHelper;
using password_manager::PasswordManagerClient;
using password_manager::PasswordManagerDriver;
using password_manager::PasswordManagerInterface;
using password_manager::metrics_util::LogPasswordDropdownShown;
using password_manager::metrics_util::PasswordDropdownState;

namespace {

// Password is considered not generated when user edits it below 4 characters.
constexpr int kMinimumLengthForEditedPassword = 4;

class PasswordAutofillAgentDelegateImpl
    : public autofill::PasswordAutofillAgentDelegate {
 public:
  ~PasswordAutofillAgentDelegateImpl() override = default;
  explicit PasswordAutofillAgentDelegateImpl(web::WebState* web_state)
      : web_state_(web_state) {}

  PasswordAutofillAgentDelegateImpl(const PasswordAutofillAgentDelegateImpl&) =
      delete;
  PasswordAutofillAgentDelegateImpl& operator=(
      const PasswordAutofillAgentDelegateImpl&) = delete;

  void DidFillField(web::WebFrame* frame,
                    std::optional<autofill::FormRendererId> form_id,
                    autofill::FieldRendererId field_id,
                    const std::u16string& field_value) override {
    auto* driver = IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(
        web_state_, frame);
    CHECK(driver);

    autofill::FieldDataManager* field_data_manager =
        autofill::FieldDataManagerFactoryIOS::FromWebFrame(frame);

    driver->GetPasswordManager()->UpdateStateOnUserInput(
        driver, *field_data_manager, form_id, field_id, field_value);
  }

 private:
  raw_ptr<web::WebState> web_state_;
};

AcceptedGeneratedPasswordSourceType DetermineGeneratedPasswordSource(
    bool proactive,
    bool manual) {
  // 'proactive' keeps track of whether the sheet was shown proactively and
  // 'manual' keeps track of whether the sheet was shown through manual
  // fallback (these are mutually exclusive events), so if both are false, it
  // means the sheet was shown through the "Suggest strong password" keyboard
  // accessory.
  return proactive ? AcceptedGeneratedPasswordSourceType::kProactiveBottomSheet
         : manual  ? AcceptedGeneratedPasswordSourceType::kManualFallback
                   : AcceptedGeneratedPasswordSourceType::kSuggestion;
}

}  // namespace

NSString* const kPasswordFormSuggestionSuffix = @" ••••••••";

@interface SharedPasswordController ()

// Helper contains common password suggestion logic.
@property(nonatomic, readonly) PasswordSuggestionHelper* suggestionHelper;

// Tracks field when current password was generated.
@property(nonatomic) FieldRendererId passwordGeneratedIdentifier;

- (BOOL)IsOffTheRecord;

// Tracks whether the potential generated password has been proactively shown
// or through the keyboard accessory
@property(nonatomic) BOOL proactivePasswordGeneration;

@end

@implementation SharedPasswordController {
  raw_ptr<PasswordManagerInterface> _passwordManager;

  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  raw_ptr<web::WebState> _webState;

  PasswordControllerDriverHelper* _driverHelper;

  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe the web frames manager from Objective-C.
  std::unique_ptr<web::WebFramesManagerObserverBridge>
      _webFramesManagerObserverBridge;

  // Bridge to observe the AutofillManagers for this `_webState`.
  std::unique_ptr<AutofillManagerObserverBridge> _autofillManagerObserverBridge;

  std::unique_ptr<base::ScopedMultiSourceObservation<AutofillManager,
                                                     AutofillManager::Observer>>
      _autofillManagerObservation;

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

  // Last focused frame.
  raw_ptr<web::WebFrame> _lastFocusedFrame;

  // A refcounted object is stored here, because otherwise the driver can
  // be deleted with the frame, and the driver needs to be alive after the
  // frame deletion for submission detecting purposes.
  scoped_refptr<IOSPasswordManagerDriver> _lastSubmittedPasswordManagerDriver;

  // Delegate for the PasswordAutofillAgent that receives information from
  // Autofill.
  std::unique_ptr<PasswordAutofillAgentDelegateImpl> _agentDelegate;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                         manager:(password_manager::PasswordManagerInterface*)
                                     passwordManager
                      formHelper:(PasswordFormHelper*)formHelper
                suggestionHelper:(PasswordSuggestionHelper*)suggestionHelper
                    driverHelper:(PasswordControllerDriverHelper*)driverHelper {
  self = [super init];
  if (self) {
    DCHECK(webState);
    IOSPasswordManagerDriverFactory::CreateForWebState(webState, self,
                                                       passwordManager);
    _agentDelegate =
        std::make_unique<PasswordAutofillAgentDelegateImpl>(webState);
    autofill::PasswordAutofillAgent::CreateForWebState(webState,
                                                       _agentDelegate.get());

    _webState = webState;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    _webFramesManagerObserverBridge =
        std::make_unique<web::WebFramesManagerObserverBridge>(self);
    web::WebFramesManager* framesManager = [self webFramesManager];
    framesManager->AddObserver(_webFramesManagerObserverBridge.get());
    _autofillManagerObserverBridge =
        std::make_unique<AutofillManagerObserverBridge>(self);
    _autofillManagerObservation =
        std::make_unique<base::ScopedMultiSourceObservation<
            AutofillManager, AutofillManager::Observer>>(
            _autofillManagerObserverBridge.get());
    _formActivityObserverBridge =
        std::make_unique<FormActivityObserverBridge>(_webState, self);
    _formHelper = formHelper;
    _formHelper.delegate = self;
    _suggestionHelper = suggestionHelper;
    _suggestionHelper.delegate = self;
    _passwordManager = passwordManager;
    _driverHelper = driverHelper;
    _proactivePasswordGeneration = NO;
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
}

- (BOOL)IsOffTheRecord {
  DCHECK(_delegate.passwordManagerClient);
  return _delegate.passwordManagerClient->IsOffTheRecord();
}

#pragma mark - PasswordGenerationProvider

- (void)triggerPasswordGeneration {
  LogPasswordGenerationEvent(
      autofill::password_generation::PASSWORD_GENERATION_CONTEXT_MENU_PRESSED);
  [self triggerPasswordGenerationForFormId:_lastFocusedFormIdentifier
                           fieldIdentifier:_lastFocusedFieldIdentifier
                                   inFrame:_lastFocusedFrame
                                 proactive:NO];
}

- (void)triggerPasswordGenerationForFormId:(FormRendererId)formIdentifier
                           fieldIdentifier:(FieldRendererId)fieldIdentifier
                                   inFrame:(web::WebFrame*)frame
                                 proactive:(BOOL)proactivePasswordGeneration {
  if (!fieldIdentifier) {
    return;
  }
  _proactivePasswordGeneration = proactivePasswordGeneration;
  // This function is reached either by using manual fallback or proactive
  // generation. Therefore, if it is not proactive password generation, it is
  // manually triggered, hence how isManuallyTriggered is set in the following
  // call.
  [self generatePasswordForFormId:formIdentifier
                  fieldIdentifier:fieldIdentifier
                          inFrame:frame
              isManuallyTriggered:!_proactivePasswordGeneration];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);

  if (!navigation->HasCommitted() || navigation->IsSameDocument()) {
    return;
  }

  if (!webState->GetLastCommittedURLIfTrusted()) {
    return;
  }

  // Clear per-page state.
  [self.suggestionHelper resetForNewPage];

  // This FieldDataManager info is for forms that were present on a page
  // before navigation, therefore not the current driver is needed, but the
  // last submitted one.
  if (_lastSubmittedPasswordManagerDriver) {
    _passwordManager->PropagateFieldDataManagerInfo(
        _lastSubmittedPasswordManagerDriver->field_data_manager(),
        _lastSubmittedPasswordManagerDriver.get());
  }
  // On non-iOS platforms navigations initiated by link click are excluded from
  // navigations which might be form submssions. On iOS there is no easy way to
  // check that the navigation is link initiated, so it is skipped. It should
  // not be so important since it is unlikely that the user clicks on a link
  // after filling password form w/o submitting it.
  _passwordManager->DidNavigateMainFrame(
      /*form_may_be_submitted=*/navigation->IsRendererInitiated());
  if (_lastSubmittedPasswordManagerDriver) {
    _lastSubmittedPasswordManagerDriver->field_data_manager().ClearData();
  }
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);

  // Retrieve the identity of the page. In case the page might be malicous,
  // returns early.
  std::optional<GURL> pageURL = webState->GetLastCommittedURLIfTrusted();
  if (!pageURL) {
    return;
  }

  if (!web::UrlHasWebScheme(*pageURL)) {
    return;
  }

  if (!webState->ContentIsHTML()) {
    // If the current page is not HTML, it does not contain any HTML forms.
    web::WebFramesManager* framesManager = [self webFramesManager];
    web::WebFrame* mainFrame = framesManager->GetMainWebFrame();
    [self didFinishPasswordFormExtraction:std::vector<FormData>()
                    triggeredByFormChange:false
                                  inFrame:mainFrame];
  }
}

- (void)webFramesManager:(web::WebFramesManager*)webFramesManager
    frameBecameAvailable:(web::WebFrame*)webFrame {
  DCHECK(webFrame);
  auto* driver =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(_webState, webFrame);
  if (driver) {
    _autofillManagerObservation->AddObservation(&driver->GetAutofillManager());
  }

  if (_webState->ContentIsHTML()) {
    [self findPasswordFormsAndSendToPasswordStoreForFormChange:false
                                                       inFrame:webFrame];
  }
}

// Track detaching iframes.
- (void)webFramesManager:(web::WebFramesManager*)webFramesManager
    frameBecameUnavailable:(const std::string&)frameId {
  // No need to try to detect submissions when the webState is being destroyed.
  if (_webState->IsBeingDestroyed()) {
    return;
  }
  web::WebFramesManager* framesManager = [self webFramesManager];
  web::WebFrame* webFrame = framesManager->GetFrameWithId(frameId);
  if (!webFrame) {
    return;
  }

  // Avoid keeping a pointer to a destroyed frame.
  if (webFrame == _lastFocusedFrame) {
    _lastFocusedFrame = nullptr;
  }

  // Main frame becomes unavailable, submission detection will happen after the
  // the new main frame is loaded.
  if (webFrame->IsMainFrame()) {
    return;
  }

  // Casting is safe, as this code is run on iOS Chrome & WebView only.
  auto* driver = static_cast<IOSPasswordManagerDriver*>(
      [_driverHelper PasswordManagerDriver:webFrame]);

  _passwordManager->OnIframeDetach(frameId, driver,
                                   driver->field_data_manager());
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    web::WebFramesManager* framesManager = [self webFramesManager];
    framesManager->RemoveObserver(_webFramesManagerObserverBridge.get());
    _webFramesManagerObserverBridge.reset();
    _formActivityObserverBridge.reset();
    _webState = nullptr;
  }
  _formGenerationData.clear();
  _isPasswordGenerated = NO;
  _lastTypedfieldIdentifier = FieldRendererId();
  _lastTypedValue = nil;
  _lastFocusedFormIdentifier = FormRendererId();
  _lastFocusedFieldIdentifier = FieldRendererId();
  _lastFocusedFrame = nullptr;
  _passwordManager = nullptr;
  _lastSubmittedPasswordManagerDriver = nullptr;
  _agentDelegate.reset();
}

#pragma mark - AutofillManagerObserver

- (void)onAutofillManagerStateChanged:(AutofillManager&)manager
                                 from:(AutofillManager::LifecycleState)oldState
                                   to:(AutofillManager::LifecycleState)
                                          newState {
  using enum autofill::AutofillManager::LifecycleState;
  switch (newState) {
    case kInactive:
    case kActive:
    case kPendingReset:
      break;
    case kPendingDeletion:
      _autofillManagerObservation->RemoveObservation(&manager);
      break;
  }
}

- (void)onFieldTypesDetermined:(AutofillManager&)manager
                       forForm:(FormGlobalId)form
                    fromSource:
                        (AutofillManager::Observer::FieldTypeSource)source {
  // Heuristics predictions are not relevant to PWM because it runs its own
  // heuristics - only server predictions are.
  if (source ==
      AutofillManager::Observer::FieldTypeSource::kHeuristicsOrAutocomplete) {
    return;
  }

  autofill::FormStructure* form_structure = manager.FindCachedFormById(form);
  if (!form_structure) {
    return;
  }
  autofill::FormDataAndServerPredictions forms_and_predictions =
      autofill::GetFormDataAndServerPredictions(*form_structure);

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillAcrossIframesIos)) {
    // Process the predictions for each renderer form that composes the browser
    // form when Autofill across frames is enabled.

    // Split the browser form into renderer forms.
    const autofill::AutofillDriverRouter& router =
        autofill::AutofillDriverIOSFactory::FromWebState(_webState)->router();
    std::vector<FormData> renderer_forms =
        router.GetRendererForms(forms_and_predictions.form_data);

    // Process predictions for each renderer form.
    web::WebFramesManager* webFramesManager = [self webFramesManager];
    for (const FormData& renderer_form : renderer_forms) {
      web::WebFrame* child_frame = webFramesManager->GetFrameWithId(
          renderer_form.host_frame().ToString());
      if (!child_frame) {
        continue;
      }
      _passwordManager->ProcessAutofillPredictions(
          IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(_webState,
                                                                   child_frame),
          renderer_form, forms_and_predictions.predictions);
    }
  } else {
    auto& driver = static_cast<autofill::AutofillDriverIOS&>(manager.driver());
    web::WebFrame* frame = driver.web_frame();
    if (!frame) {
      return;
    }
    // `GetFormDataAndServerPredictions` returns the same number of `FormData`
    // as `FormStructure` that are passed to it, i.e. one in this case.
    // Therefore take the front.
    _passwordManager->ProcessAutofillPredictions(
        IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(_webState,
                                                                 frame),
        forms_and_predictions.form_data, forms_and_predictions.predictions);
  }
}

#pragma mark - FormSuggestionProvider

- (void)checkIfSuggestionsAvailableForForm:
            (FormSuggestionProviderQuery*)formQuery
                            hasUserGesture:(BOOL)hasUserGesture
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  DCHECK_EQ(_webState, webState);
  if (!webState->GetLastCommittedURLIfTrusted()) {
    completion(NO);
    return;
  }

  password_manager::PasswordManagerJavaScriptFeature* feature =
      password_manager::PasswordManagerJavaScriptFeature::GetInstance();
  web::WebFrame* frame = feature->GetWebFramesManager(webState)->GetFrameWithId(
      SysNSStringToUTF8(formQuery.frameID));

  // Clicking on a password form field from a different form on the same page
  // triggers displaying the on-screen keyboard. When the keyboard is
  // displayed, FormInputAccessoryMediator uses the cached parameters from the
  // previous clicked field in the previous password form. Getting the frame
  // from this previous frame id will result in a null frame pointer, hence
  // the check below.
  if (!frame) {
    completion(NO);
    return;
  }

  BOOL isPasswordField = [self.suggestionHelper isPasswordFieldOnForm:formQuery
                                                             webFrame:frame];

  [self.suggestionHelper
      checkIfSuggestionsAvailableForForm:formQuery
                       completionHandler:^(BOOL suggestionsAvailable) {
                         // Always display "Show All..." for password fields.
                         completion(isPasswordField || suggestionsAvailable);
                       }];

  if (self.isPasswordGenerated &&
      ([formQuery.type isEqual:@"input"] ||
       [formQuery.type isEqual:@"keyup"]) &&
      formQuery.fieldRendererID == self.passwordGeneratedIdentifier) {
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
      _passwordManager->OnPasswordNoLongerGenerated();
    } else {
      // Inject updated value to possibly update confirmation field.
      [self injectGeneratedPasswordForFormId:formQuery.formRendererID
                                     inFrame:frame
                           generatedPassword:formQuery.typedValue
                           completionHandler:nil];
    }
  }

  if (formQuery.fieldRendererID != _lastTypedfieldIdentifier ||
      ![formQuery.typedValue isEqual:_lastTypedValue]) {
    // This method is called multiple times for the same user keystroke. Inform
    // only once the keystroke.
    _lastTypedfieldIdentifier = formQuery.fieldRendererID;
    _lastTypedValue = formQuery.typedValue;

    if ([formQuery.type isEqual:@"input"] ||
        [formQuery.type isEqual:@"keyup"]) {
      [self.formHelper updateFieldDataOnUserInput:formQuery.fieldRendererID
                                          inFrame:frame
                                       inputValue:formQuery.typedValue];
      autofill::FieldDataManager* fieldDataManager =
          autofill::FieldDataManagerFactoryIOS::FromWebFrame(frame);
      _passwordManager->UpdateStateOnUserInput(
          [_driverHelper PasswordManagerDriver:frame], *fieldDataManager,
          formQuery.formRendererID, formQuery.fieldRendererID,
          SysNSStringToUTF16(formQuery.typedValue));
    }
  }
}

- (void)retrieveSuggestionsForForm:(FormSuggestionProviderQuery*)formQuery
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  DCHECK_EQ(_webState, webState);
  if (!webState->GetLastCommittedURLIfTrusted()) {
    completion({}, self);
    return;
  }

  const std::string frameId = SysNSStringToUTF8(formQuery.frameID);
  web::WebFramesManager* framesManager = [self webFramesManager];
  web::WebFrame* frame = framesManager->GetFrameWithId(frameId);

  if (frame == nullptr) {
    completion({}, self);
    return;
  }
  NSArray<FormSuggestion*>* rawSuggestions =
      [self.suggestionHelper retrieveSuggestionsWithForm:formQuery];

  NSMutableArray<FormSuggestion*>* suggestions = [NSMutableArray array];
  bool isPasswordField = [self.suggestionHelper isPasswordFieldOnForm:formQuery
                                                             webFrame:frame];
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
    NSString* value = [rawSuggestion.value
        stringByAppendingString:kPasswordFormSuggestionSuffix];
    FormSuggestion* suggestion =
        [FormSuggestion suggestionWithValue:value
                         displayDescription:rawSuggestion.displayDescription
                                       icon:nil
                                       type:rawSuggestion.type
                          backendIdentifier:nil
                             requiresReauth:YES
                 acceptanceA11yAnnouncement:nil
                                   metadata:rawSuggestion.metadata];
    [suggestions addObject:suggestion];
  }

  if ([self canGeneratePasswordForForm:formQuery.formRendererID
                       fieldIdentifier:formQuery.fieldRendererID
                             fieldType:formQuery.fieldType
                               inFrame:frame]) {
    NSString* suggestPassword = GetNSString(IDS_IOS_SUGGEST_PASSWORD);
    FormSuggestion* suggestion = [FormSuggestion
        suggestionWithValue:suggestPassword
         displayDescription:nil
                       icon:nil
                       type:autofill::SuggestionType::kGeneratePasswordEntry
          backendIdentifier:nil
             requiresReauth:NO];

    [suggestions addObject:suggestion];
  }

  std::vector<autofill::Suggestion> suggestions_vector;
  for (FormSuggestion* suggestion in suggestions) {
    // LogPasswordDropdownShown requires types only.
    suggestions_vector.emplace_back(suggestion.type);
  }
  LogPasswordDropdownShown(suggestions_vector);

  completion(suggestions, self);
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                    atIndex:(NSInteger)index
                       form:(NSString*)formName
             formRendererID:(FormRendererId)formRendererID
            fieldIdentifier:(NSString*)fieldIdentifier
            fieldRendererID:(FieldRendererId)fieldRendererID
                    frameID:(NSString*)frameID
          completionHandler:(SuggestionHandledCompletion)completion {
  const std::string frameId = SysNSStringToUTF8(frameID);
  web::WebFramesManager* framesManager = [self webFramesManager];
  web::WebFrame* frame = framesManager->GetFrameWithId(frameId);
  if (!frame) {
    completion();
    return;
  }

  switch (suggestion.type) {
    case autofill::SuggestionType::kAllSavedPasswordsEntry: {
      completion();
      password_manager::metrics_util::LogPasswordDropdownItemSelected(
          password_manager::metrics_util::PasswordDropdownSelectedOption::
              kShowAll,
          [self IsOffTheRecord]);
      return;
    }
    case autofill::SuggestionType::kGeneratePasswordEntry: {
      // Don't call completion because current suggestion state should remain
      // whether user injects a generated password or cancels.
      _proactivePasswordGeneration = NO;
      [self generatePasswordForFormId:formRendererID
                      fieldIdentifier:fieldRendererID
                              inFrame:frame
                  isManuallyTriggered:NO];
      password_manager::metrics_util::LogPasswordDropdownItemSelected(
          password_manager::metrics_util::PasswordDropdownSelectedOption::
              kGenerate,
          [self IsOffTheRecord]);
      return;
    }
    default: {
      password_manager::metrics_util::LogPasswordDropdownItemSelected(
          password_manager::metrics_util::PasswordDropdownSelectedOption::
              kPassword,
          [self IsOffTheRecord]);
      DCHECK([suggestion.value hasSuffix:kPasswordFormSuggestionSuffix]);
      NSString* username = [suggestion.value
          substringToIndex:suggestion.value.length -
                           kPasswordFormSuggestionSuffix.length];
      std::unique_ptr<password_manager::FillData> fillData =
          [self.suggestionHelper passwordFillDataForUsername:username
                                                  forFrameId:frameId];

      if (!fillData) {
        completion();
        return;
      }

      [self.formHelper fillPasswordFormWithFillData:*fillData
                                            inFrame:frame
                                   triggeredOnField:fieldRendererID
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

- (autofill::FillingProduct)mainFillingProduct {
  return autofill::FillingProduct::kPassword;
}

#pragma mark - PasswordManagerDriverDelegate

- (const GURL&)lastCommittedURL {
  return _webState ? _webState->GetLastCommittedURL() : GURL::EmptyGURL();
}

- (void)processPasswordFormFillData:
            (const autofill::PasswordFormFillData&)formData
                         forFrameId:(const std::string&)frameId
                        isMainFrame:(BOOL)isMainFrame
                  forSecurityOrigin:(const GURL&)origin {
  // Biometric auth is always enabled on iOS so wait_for_username is
  // specifically set to prevent filling without user confirmation.
  DCHECK(formData.wait_for_username);
  [self.suggestionHelper processWithPasswordFormFillData:formData
                                              forFrameId:frameId
                                             isMainFrame:isMainFrame
                                       forSecurityOrigin:origin];
}

- (void)onNoSavedCredentialsWithFrameId:(const std::string&)frameId {
  [self.suggestionHelper processWithNoSavedCredentialsWithFrameId:frameId];
  [self detachListenersForBottomSheet:frameId];
}

- (void)formEligibleForGenerationFound:(const PasswordFormGenerationData&)form {
  _formGenerationData[form.form_renderer_id] = form;
}

- (void)attachListenersForPasswordGenerationFields:
            (const PasswordFormGenerationData&)form
                                        forFrameId:(const std::string&)frameId {
  const std::vector<autofill::FieldRendererId> rendererIds = {
      form.new_password_renderer_id};
  [self.delegate attachListenersForPasswordGenerationBottomSheet:rendererIds
                                                      forFrameId:frameId];
}

#pragma mark - PasswordFormHelperDelegate

- (void)formHelper:(PasswordFormHelper*)formHelper
     didSubmitForm:(const FormData&)form
           inFrame:(web::WebFrame*)frame {
  DCHECK(frame);

  IOSPasswordManagerDriver* driver =
      [_driverHelper PasswordManagerDriver:frame];
  if (frame->IsMainFrame()) {
    _passwordManager->OnPasswordFormSubmitted(driver, form);
  } else {
    // Show a save prompt immediately because for iframes it is very hard to
    // figure out correctness of password forms submission.
    _passwordManager->OnSubframeFormSubmission(driver, form);
  }
}

#pragma mark - PasswordSuggestionHelperDelegate

- (void)suggestionHelperShouldTriggerFormExtraction:
            (PasswordSuggestionHelper*)suggestionHelper
                                            inFrame:(web::WebFrame*)frame {
  [self findPasswordFormsAndSendToPasswordStoreForFormChange:false
                                                     inFrame:frame];
}

- (void)attachListenersForBottomSheet:
            (const std::vector<autofill::FieldRendererId>&)rendererIds
                           forFrameId:(const std::string&)frameId {
  [self.delegate attachListenersForBottomSheet:rendererIds forFrameId:frameId];
}

- (void)detachListenersForBottomSheet:(const std::string&)frameId {
  [self.delegate detachListenersForBottomSheet:frameId];
}

#pragma mark - Private methods

- (void)didFinishPasswordFormExtraction:(const std::vector<FormData>&)forms
                  triggeredByFormChange:(BOOL)triggeredByFormChange
                                inFrame:(web::WebFrame*)frame {
  // Do nothing if |self| has been detached.
  if (!_passwordManager) {
    return;
  }
  IOSPasswordManagerDriver* driver =
      [_driverHelper PasswordManagerDriver:frame];
  if (!forms.empty()) {
    // Invoke the password manager callback to autofill password forms
    // on the loaded page.
    _passwordManager->OnPasswordFormsParsed(driver, forms);
  } else if (frame) {
    [self onNoSavedCredentialsWithFrameId:frame->GetFrameId()];
  }
  // Invoke the password manager callback to check if password was
  // accepted or rejected. If accepted, infobar is presented. If
  // rejected, the provisionally saved password is deleted. On Chrome
  // w/ a renderer, it is the renderer who calls OnPasswordFormsParsed()
  // and OnPasswordFormsRendered(). Bling has to improvised a bit on the
  // ordering of these two calls.
  // Only check for form submissions if forms are not being parsed due to
  // added elements to the form.
  if (!triggeredByFormChange) {
    _passwordManager->OnPasswordFormsRendered(driver, forms);
  }
}

- (void)findPasswordFormsAndSendToPasswordStoreForFormChange:
            (BOOL)triggeredByFormChange
                                                     inFrame:
                                                         (web::WebFrame*)frame {
  // Read all password forms from the page and send them to the password
  // manager.
  __weak SharedPasswordController* weakSelf = self;
  auto completionHandler = ^(const std::vector<FormData>& forms) {
    [weakSelf didFinishPasswordFormExtraction:forms
                        triggeredByFormChange:triggeredByFormChange
                                      inFrame:frame];
  };

  [self.formHelper findPasswordFormsInFrame:frame
                          completionHandler:completionHandler];
}

- (BOOL)canGeneratePasswordForForm:(FormRendererId)formIdentifier
                   fieldIdentifier:(FieldRendererId)fieldIdentifier
                         fieldType:(NSString*)fieldType
                           inFrame:(web::WebFrame*)frame {
  if (![_driverHelper PasswordGenerationHelper:frame]->IsGenerationEnabled(
          /*log_debug_data*/ true)) {
    return NO;
  }
  if (![fieldType isEqual:kObfuscatedFieldType]) {
    return NO;
  }
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

- (void)formDataCompletionForForm:(const autofill::FormData&)form
               formSignatureFound:(BOOL)found
              isManuallyTriggered:(BOOL)isManuallyTriggered
                   formIdentifier:(FormRendererId)formIdentifier
                  fieldIdentifier:(FieldRendererId)fieldIdentifier
                            frame:(base::WeakPtr<web::WebFrame>)weakFrame {
  if (!_webState) {
    // Stop here if the '_webState' was deleted before handling the decision for
    // the generated password. Doing anything further is unsafe.
    return;
  }
  web::WebFrame* frame = weakFrame.get();
  if (!frame) {
    // The frame has been destroyed, probably due to a
    // navigation or closing the tab, ignore the event.
    return;
  }

  autofill::FormSignature formSignature =
      found ? CalculateFormSignature(form) : autofill::FormSignature(0);
  autofill::FieldSignature fieldSignature = autofill::FieldSignature(0);
  uint64_t maxLength = 0;

  if (found) {
    for (const autofill::FormFieldData& field : form.fields()) {
      if (field.renderer_id() == fieldIdentifier) {
        fieldSignature = CalculateFieldSignatureForField(field);
        maxLength = field.max_length();
        break;
      }
    }
  }

  std::u16string generatedPassword =
      [_driverHelper PasswordGenerationHelper:frame]->GeneratePassword(
          [self lastCommittedURL],
          isManuallyTriggered ? PasswordGenerationType::kManual
                              : PasswordGenerationType::kAutomatic,
          formSignature, fieldSignature, maxLength);

  base::UmaHistogramBoolean(
      "PasswordGeneration.iOS.GeneratedPasswordIsEmpty.BeforeParsing",
      generatedPassword.empty());

  NSString* generatedPotentialPassword = SysUTF16ToNSString(generatedPassword);

  __weak SharedPasswordController* weakSelf = self;
  [self.delegate sharedPasswordController:self
           showGeneratedPotentialPassword:generatedPotentialPassword
                                proactive:self.proactivePasswordGeneration
                          decisionHandler:^(BOOL accept) {
                            [weakSelf
                                onGeneratedPasswordAccepted:
                                    generatedPotentialPassword
                                                 isAccepted:accept
                                        isManuallyTriggered:isManuallyTriggered
                                             formIdentifier:formIdentifier
                                                      frame:weakFrame];
                          }];
}

- (void)onGeneratedPasswordAccepted:(NSString*)generatedPassword
                         isAccepted:(BOOL)accepted
                isManuallyTriggered:(BOOL)isManuallyTriggered
                     formIdentifier:(FormRendererId)formIdentifier
                              frame:(base::WeakPtr<web::WebFrame>)weakFrame {
  if (!_webState) {
    // Stop here if the '_webState' was deleted before handling the decision for
    // the generated password. Doing anything further is unsafe.
    return;
  }
  web::WebFrame* frame = weakFrame.get();
  if (!frame) {
    // The frame has been destroyed, probably due to a
    // navigation or closing the tab, ignore the event.
    return;
  }

  [self logUserChoice:accepted
            proactive:self.proactivePasswordGeneration
               manual:isManuallyTriggered];
  bool generatedPasswordEmpty = generatedPassword.length == 0;
  if (accepted) {
    // Log "on accepted" metrics which doesn't necessarily mean that the generated
    // password can be injected.
    LogPasswordGenerationEvent(
        autofill::password_generation::PASSWORD_ACCEPTED);
    base::UmaHistogramBoolean(
        "PasswordGeneration.iOS.AcceptedGeneratedPasswordIsEmpty",
        generatedPasswordEmpty);
  }
  if (accepted && !generatedPasswordEmpty) {
    // Only inject the generated password if not empty, password manager will
    // trigger a crash otherwise.
    [self injectGeneratedPasswordForFormId:formIdentifier
                                   inFrame:frame
                         generatedPassword:generatedPassword
                         completionHandler:nil];
  } else {
    [self cancelPasswordGeneration];
  }
}

- (void)cancelPasswordGeneration {
  _passwordManager->OnPasswordNoLongerGenerated();
}

- (void)generatePasswordForFormId:(FormRendererId)formIdentifier
                  fieldIdentifier:(FieldRendererId)fieldIdentifier
                          inFrame:(web::WebFrame*)frame
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
    PasswordFormGenerationData newGenerationData = {
        .form_renderer_id = formIdentifier,
        .new_password_renderer_id = fieldIdentifier,
    };
    [self formEligibleForGenerationFound:newGenerationData];
  }

  __weak SharedPasswordController* weakSelf = self;
  base::WeakPtr<web::WebFrame> weakFrame = frame->AsWeakPtr();
  [self.formHelper
      extractPasswordFormData:formIdentifier
                      inFrame:frame
            completionHandler:^(BOOL found, const autofill::FormData& form) {
              [weakSelf formDataCompletionForForm:form
                               formSignatureFound:found
                              isManuallyTriggered:isManuallyTriggered
                                   formIdentifier:formIdentifier
                                  fieldIdentifier:fieldIdentifier
                                            frame:weakFrame];
            }];

  IOSPasswordManagerDriver* driver =
      [_driverHelper PasswordManagerDriver:frame];
  _passwordManager->SetGenerationElementAndTypeForForm(
      driver, formIdentifier, fieldIdentifier,
      isManuallyTriggered ? PasswordGenerationType::kManual
                          : PasswordGenerationType::kAutomatic);
}

- (void)injectGeneratedPasswordForFormId:(FormRendererId)formIdentifier
                                 inFrame:(web::WebFrame*)frame
                       generatedPassword:(NSString*)generatedPassword
                       completionHandler:(void (^)())completionHandler {
  const autofill::PasswordFormGenerationData* generationData =
      [self formForGenerationFromFormID:formIdentifier];
  CHECK_GT(generatedPassword.length, 0u);
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
                    passwordUniqueId:newPasswordUniqueId
                             inFrame:frame];
    }
    if (completionHandler) {
      completionHandler();
    }
  };

  [self.formHelper fillPasswordForm:formIdentifier
                            inFrame:frame
              newPasswordIdentifier:newPasswordUniqueId
          confirmPasswordIdentifier:confirmPasswordUniqueId
                  generatedPassword:generatedPassword
                  completionHandler:generatedPasswordInjected];
}

- (void)onFilledPasswordForm:(FormRendererId)formIdentifier
       withGeneratedPassword:(NSString*)generatedPassword
            passwordUniqueId:(FieldRendererId)newPasswordUniqueId
                     inFrame:(web::WebFrame*)frame {
  __weak SharedPasswordController* weakSelf = self;
  auto passwordPresaved = ^(BOOL found, const autofill::FormData& form) {
    // If the form isn't found, it disappeared between the call to
    // [self.formHelper fillPasswordForm:newPasswordIdentifier:...]
    // and here. There isn't much that can be done.
    if (!found)
      return;

    [weakSelf presaveGeneratedPassword:generatedPassword
                              formData:form
                               inFrame:frame];
  };

  [self.formHelper extractPasswordFormData:formIdentifier
                                   inFrame:frame
                         completionHandler:passwordPresaved];
  self.isPasswordGenerated = YES;
  self.passwordGeneratedIdentifier = newPasswordUniqueId;
}

- (void)presaveGeneratedPassword:(NSString*)generatedPassword
                        formData:(const autofill::FormData&)formData
                         inFrame:(web::WebFrame*)frame {
  if (!_passwordManager)
    return;

  _passwordManager->OnPresaveGeneratedPassword(
      [_driverHelper PasswordManagerDriver:frame], formData,
      SysNSStringToUTF16(generatedPassword));
}

- (web::WebFramesManager*)webFramesManager {
  return password_manager::PasswordManagerJavaScriptFeature::GetInstance()
      ->GetWebFramesManager(_webState);
}

#pragma mark - FormActivityObserver

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  DCHECK_EQ(_webState, webState);

  std::optional<GURL> pageURL = webState->GetLastCommittedURLIfTrusted();
  if (!pageURL || !frame || params.input_missing) {
    _lastFocusedFormIdentifier = FormRendererId();
    _lastFocusedFieldIdentifier = FieldRendererId();
    _lastFocusedFrame = nullptr;
    return;
  }

  if (params.type == "input" || params.type == "change") {
    _lastSubmittedPasswordManagerDriver =
        IOSPasswordManagerDriverFactory::GetRetainableDriver(_webState, frame);
  }

  if (params.type == "focus") {
    _lastFocusedFormIdentifier = params.form_renderer_id;
    _lastFocusedFieldIdentifier = params.field_renderer_id;
    _lastFocusedFrame = frame;
  }

  // If there's a change in password forms on a page, they should be parsed
  // again.
  if (params.type == "form_changed") {
    [self findPasswordFormsAndSendToPasswordStoreForFormChange:true
                                                       inFrame:frame];
  }
}

// If the form was removed, PasswordManager should be informed to decide
// whether the form was submitted.
- (void)webState:(web::WebState*)webState
    didRegisterFormRemoval:(const autofill::FormRemovalParams&)params
                   inFrame:(web::WebFrame*)frame {
  CHECK_EQ(_webState, webState);
  CHECK(!params.removed_forms.empty() || !params.removed_unowned_fields.empty())
      << "Invalid params. Form removal events with missing input should have "
         "been filtered out by FormActivityTabHelper.";

  auto* driver = static_cast<IOSPasswordManagerDriver*>(
      [_driverHelper PasswordManagerDriver:frame]);

  _passwordManager->OnPasswordFormsRemoved(
      driver, driver->field_data_manager(),
      /*removed_forms=*/params.removed_forms,
      /*removed_unowned_fields=*/params.removed_unowned_fields);
}

- (void)logUserChoice:(BOOL)accept
            proactive:(BOOL)proactive
               manual:(BOOL)manual {
  if (accept) {
    if (proactive) {
      // Suggested generated password accepted on proactive bottom sheet.
      base::UmaHistogramEnumeration(
          "PasswordManager.TouchToFill.PasswordGeneration.UserChoice",
          password_manager::metrics_util::GenerationDialogChoice::kAccepted);
    }
    base::UmaHistogramEnumeration(
        "PasswordGeneration.iOS.AcceptedGeneratedPasswordSource",
        DetermineGeneratedPasswordSource(proactive, manual));
  } else if (proactive) {
    // Suggested generated password is not accepted from proactive bottom sheet.
    base::UmaHistogramEnumeration(
        "PasswordManager.TouchToFill."
        "PasswordGeneration.UserChoice",
        password_manager::metrics_util::GenerationDialogChoice::kRejected);
  }
}

@end
