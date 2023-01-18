// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_agent.h"

#import <UIKit/UIKit.h>

#include <memory>
#include <string>
#include <utility>

#include "base/format_macros.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/mac/foundation_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#include "components/autofill/ios/browser/autofill_driver_ios_webframe.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#include "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/ios/ukm_url_recorder.h"
#include "ios/web/common/url_scheme_util.h"
#include "ios/web/public/deprecated/url_verification_constants.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FieldDataManager;
using autofill::FieldRendererId;
using autofill::FormGlobalId;
using autofill::FormRendererId;
using autofill::FieldPropertiesFlags::kAutofilledOnUserTrigger;
using base::NumberToString;
using base::SysNSStringToUTF16;
using base::SysNSStringToUTF8;
using base::SysUTF16ToNSString;

namespace {

using FormDataVector = std::vector<autofill::FormData>;

// The type of the completion handler block for
// |fetchFormsWithName:minimumRequiredFieldsCount:completionHandler|
typedef void (^FetchFormsCompletionHandler)(BOOL, const FormDataVector&);

// Gets the field specified by |fieldIdentifier| from |form|, if focusable. Also
// modifies the field's value for the select elements.
void GetFormField(autofill::FormFieldData* field,
                  const autofill::FormData& form,
                  FieldRendererId fieldIdentifier) {
  for (const auto& currentField : form.fields) {
    if (currentField.unique_renderer_id == fieldIdentifier &&
        currentField.is_focusable) {
      *field = currentField;
      break;
    }
  }
  if (field->SameFieldAs(autofill::FormFieldData()))
    return;

  // Hack to get suggestions from select input elements.
  if (field->form_control_type == "select-one") {
    // Any value set will cause the BrowserAutofillManager to filter suggestions
    // (only show suggestions that begin the same as the current value) with the
    // effect that one only suggestion would be returned; the value itself.
    field->value = std::u16string();
  }
}

// Delay for setting an utterance to be queued, it is required to ensure that
// standard announcements have already been started and thus would not interrupt
// the enqueued utterance.
constexpr base::TimeDelta kA11yAnnouncementQueueDelay = base::Seconds(1);

}  // namespace

@interface AutofillAgent () <CRWWebStateObserver,
                             FormActivityObserver,
                             PrefObserverDelegate> {
  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* _webState;

  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // The pref service for which this agent was created.
  PrefService* _prefService;

  // The unique renderer ID of the most recent autocomplete field;
  // tracks the currently-focused form element in order to force filling of
  // the currently selected form element, even if it's non-empty.
  FieldRendererId _pendingAutocompleteFieldID;

  // Suggestions state:
  // The most recent form suggestions.
  NSArray* _mostRecentSuggestions;

  // The completion to inform FormSuggestionController that a user selection
  // has been handled.
  SuggestionHandledCompletion _suggestionHandledCompletion;

  // The completion to inform FormSuggestionController that suggestions are
  // available for a given form and field.
  SuggestionsAvailableCompletion _suggestionsAvailableCompletion;

  // The text entered by the user into the active field.
  NSString* _typedValue;

  // Popup delegate for the most recent suggestions.
  // The reference is weak because a weak pointer is sent to our
  // BrowserAutofillManagerDelegate.
  base::WeakPtr<autofill::AutofillPopupDelegate> _popupDelegate;

  // The autofill data that needs to be send when the |webState_| is shown.
  // The pair contains the frame ID and the base::Value::Dict to send.
  // If the value is nullopt, no data needs to be sent.
  absl::optional<std::pair<std::string, base::Value::Dict>> _pendingFormData;

  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // Bridge to observe form activity in |webState_|.
  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;

  // AutofillDriverIOSWebFrame will keep a refcountable AutofillDriverIOS.
  // This is a workaround crbug.com/892612. On submission,
  // AutofillDownloadManager and CreditCardSaveManager expect
  // BrowserAutofillManager and AutofillDriver to live after web frame deletion
  // so AutofillAgent will keep the latest submitted AutofillDriver alive.
  // TODO(crbug.com/892612): remove this workaround once life cycle of
  // BrowserAutofillManager is fixed.
  scoped_refptr<autofill::AutofillDriverIOSRefCountable>
      _last_submitted_autofill_driver;

  scoped_refptr<FieldDataManager> _fieldDataManager;

  // ID of the last Autofill query made. Used to discard outdated suggestions.
  autofill::FieldGlobalId _lastQueriedFieldID;
}

@end

@implementation AutofillAgent

- (instancetype)initWithPrefService:(PrefService*)prefService
                           webState:(web::WebState*)webState {
  DCHECK(prefService);
  DCHECK(webState);
  self = [super init];
  if (self) {
    _webState = webState;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    _formActivityObserverBridge =
        std::make_unique<autofill::FormActivityObserverBridge>(_webState, self);
    _prefService = prefService;
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefChangeRegistrar.Init(prefService);
    _prefObserverBridge->ObserveChangesForPreference(
        autofill::prefs::kAutofillCreditCardEnabled, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        autofill::prefs::kAutofillProfileEnabled, &_prefChangeRegistrar);

    UniqueIDDataTabHelper* uniqueIDDataTabHelper =
        UniqueIDDataTabHelper::FromWebState(_webState);
    _fieldDataManager = uniqueIDDataTabHelper->GetFieldDataManager();
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    [self webStateDestroyed:_webState];
  }
}

#pragma mark - Private methods

// Returns the autofill manager associated with a web::WebState instance.
// Returns nullptr if there is no autofill manager associated anymore, this can
// happen when |close| has been called on the |webState|. Also returns nullptr
// if -webStateDestroyed: has been called.
- (autofill::BrowserAutofillManager*)
    autofillManagerFromWebState:(web::WebState*)webState
                       webFrame:(web::WebFrame*)webFrame {
  if (!webState || !_webStateObserverBridge)
    return nullptr;
  return autofill::AutofillDriverIOS::FromWebStateAndWebFrame(webState,
                                                              webFrame)
      ->autofill_manager();
}

// Notifies the autofill manager when forms are detected on a page.
- (void)notifyBrowserAutofillManager:
            (autofill::BrowserAutofillManager*)autofillManager
                         ofFormsSeen:(const FormDataVector&)updated_forms {
  DCHECK(autofillManager);
  DCHECK(!updated_forms.empty());
  // TODO(crbug.com/1215337): Notify |autofillManager| about deleted fields.
  std::vector<FormGlobalId> removed_forms;
  autofillManager->OnFormsSeen(/*updated_forms=*/updated_forms,
                               /*removed_forms=*/removed_forms);
}

// Notifies the autofill manager when forms are submitted.
- (void)notifyBrowserAutofillManager:
            (autofill::BrowserAutofillManager*)autofillManager
                    ofFormsSubmitted:(const FormDataVector&)forms
                       userInitiated:(BOOL)userInitiated {
  DCHECK(autofillManager);
  // Exactly one form should be extracted.
  DCHECK_EQ(1U, forms.size());
  autofill::FormData form = forms[0];
  autofillManager->OnFormSubmitted(
      form, false, autofill::mojom::SubmissionSource::FORM_SUBMISSION);
}

// Invokes the form extraction script in |frame| and loads the output into the
// format expected by the BrowserAutofillManager.
// If |filtered| is NO, all forms are extracted.
// If |filtered| is YES,
//   - if |formName| is non-empty, only a form of that name is extracted.
//   - if |formName| is empty, unowned fields are extracted.
// Only forms with at least |requiredFieldsCount| fields are extracted.
// Calls |completionHandler| with a success BOOL of YES and the form data that
// was extracted.
// Calls |completionHandler| with NO if the forms could not be extracted.
// |completionHandler| cannot be nil.
- (void)fetchFormsFiltered:(BOOL)filtered
                      withName:(const std::u16string&)formName
    minimumRequiredFieldsCount:(NSUInteger)requiredFieldsCount
                       inFrame:(web::WebFrame*)frame
             completionHandler:(FetchFormsCompletionHandler)completionHandler {
  DCHECK(completionHandler);

  // Necessary so the values can be used inside a block.
  std::u16string formNameCopy = formName;
  GURL pageURL = _webState->GetLastCommittedURL();
  GURL frameOrigin =
      frame ? frame->GetSecurityOrigin() : pageURL.DeprecatedGetOriginAsURL();
  autofill::AutofillJavaScriptFeature::GetInstance()->FetchForms(
      frame, requiredFieldsCount, base::BindOnce(^(NSString* formJSON) {
        std::vector<autofill::FormData> formData;
        bool success = autofill::ExtractFormsData(
            formJSON, filtered, formNameCopy, pageURL, frameOrigin, &formData);
        completionHandler(success, formData);
      }));
}

- (void)onSuggestionsReady:(NSArray<FormSuggestion*>*)suggestions
             popupDelegate:
                 (const base::WeakPtr<autofill::AutofillPopupDelegate>&)
                     delegate {
  _popupDelegate = delegate;
  _mostRecentSuggestions = suggestions;
  if (_suggestionsAvailableCompletion)
    _suggestionsAvailableCompletion([_mostRecentSuggestions count] > 0);
  _suggestionsAvailableCompletion = nil;
}

#pragma mark - FormSuggestionProvider

// Sends a request to BrowserAutofillManager to retrieve suggestions for the
// specified form and field.
- (void)queryAutofillForForm:(const autofill::FormData&)form
             fieldIdentifier:(FieldRendererId)fieldIdentifier
                        type:(NSString*)type
                  typedValue:(NSString*)typedValue
                     frameID:(NSString*)frameID
                    webState:(web::WebState*)webState
           completionHandler:(SuggestionsAvailableCompletion)completion {
  web::WebFrame* frame =
      GetWebFrameWithId(webState, SysNSStringToUTF8(frameID));
  autofill::BrowserAutofillManager* autofillManager =
      [self autofillManagerFromWebState:webState webFrame:frame];
  if (!autofillManager)
    return;

  // Find the right field.
  autofill::FormFieldData field;
  GetFormField(&field, form, fieldIdentifier);

  // Save the completion and go look for suggestions.
  _suggestionsAvailableCompletion = [completion copy];
  _typedValue = [typedValue copy];

  // Query the BrowserAutofillManager for suggestions. Results will arrive in
  // -showAutofillPopup:popupDelegate:.
  _lastQueriedFieldID = field.global_id();
  autofillManager->OnAskForValuesToFill(
      form, field, gfx::RectF(), autofill::AutoselectFirstSuggestion(false),
      autofill::FormElementWasClicked(false));
}

- (void)checkIfSuggestionsAvailableForForm:
            (FormSuggestionProviderQuery*)formQuery
                            hasUserGesture:(BOOL)hasUserGesture
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  DCHECK_EQ(_webState, webState);

  if (![self isAutofillEnabled]) {
    completion(NO);
    return;
  }

  // Check for suggestions if the form activity is initiated by the user.
  if (!hasUserGesture) {
    completion(NO);
    return;
  }

  web::WebFrame* frame =
      web::GetWebFrameWithId(_webState, SysNSStringToUTF8(formQuery.frameID));
  if (!frame) {
    completion(NO);
    return;
  }

  // Once the active form and field are extracted, send a query to the
  // BrowserAutofillManager for suggestions.
  __weak AutofillAgent* weakSelf = self;
  id completionHandler = ^(BOOL success, const FormDataVector& forms) {
    if (success && forms.size() == 1) {
      [weakSelf queryAutofillForForm:forms[0]
                     fieldIdentifier:formQuery.uniqueFieldID
                                type:formQuery.type
                          typedValue:formQuery.typedValue
                             frameID:formQuery.frameID
                            webState:webState
                   completionHandler:completion];
    }
  };

  // Re-extract the active form and field only. All forms with at least one
  // input element are considered because key/value suggestions are offered
  // even on short forms.
  [self fetchFormsFiltered:YES
                        withName:SysNSStringToUTF16(formQuery.formName)
      minimumRequiredFieldsCount:1
                         inFrame:frame
               completionHandler:completionHandler];
}

- (void)retrieveSuggestionsForForm:(FormSuggestionProviderQuery*)formQuery
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  DCHECK(_mostRecentSuggestions) << "Requestor should have called "
                                 << "|checkIfSuggestionsAvailableForForm:"
                                    "webState:completionHandler:|.";
  completion(_mostRecentSuggestions, self);
}

- (void)updateFieldManagerForClearedIDs:(NSString*)jsonString {
  std::vector<FieldRendererId> clearingResults;
  if (autofill::ExtractIDs(jsonString, &clearingResults)) {
    for (auto uniqueID : clearingResults) {
      _fieldDataManager->UpdateFieldDataMap(uniqueID, std::u16string(),
                                            kAutofilledOnUserTrigger);
    }
  }
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                       form:(NSString*)formName
               uniqueFormID:(FormRendererId)uniqueFormID
            fieldIdentifier:(NSString*)fieldIdentifier
              uniqueFieldID:(FieldRendererId)uniqueFieldID
                    frameID:(NSString*)frameID
          completionHandler:(SuggestionHandledCompletion)completion {
  [[UIDevice currentDevice] playInputClick];
  DCHECK(completion);
  _suggestionHandledCompletion = [completion copy];

  if (suggestion.acceptanceA11yAnnouncement != nil) {
    __weak AutofillAgent* weakSelf = self;
    // The announcement is done asyncronously with certain delay to make sure
    // it is not interrupted by (almost) immediate standard announcements.
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW,
                      kA11yAnnouncementQueueDelay.InNanoseconds()),
        dispatch_get_main_queue(), ^{
          AutofillAgent* strongSelf = weakSelf;
          if (!strongSelf)
            return;

          // Queueing flag allows to preserve standard announcements, they
          // are conveyed first and then announce this message.
          // This is a tradeoff as there is no control over the standard
          // utterances (they are interrupting) and it is not desirable
          // to interrupt them. Hence acceptance announcement is done after
          // standard ones (which takes seconds).
          NSAttributedString* message = [[NSAttributedString alloc]
              initWithString:suggestion.acceptanceA11yAnnouncement
                  attributes:@{
                    UIAccessibilitySpeechAttributeQueueAnnouncement : @YES
                  }];
          UIAccessibilityPostNotification(
              UIAccessibilityAnnouncementNotification, message);
        });
  }

  if (suggestion.identifier > 0) {
    _pendingAutocompleteFieldID = uniqueFieldID;
    if (_popupDelegate) {
      // TODO(966411): Replace 0 with the index of the selected suggestion.
      autofill::Suggestion autofill_suggestion;
      autofill_suggestion.main_text.value =
          SysNSStringToUTF16(suggestion.value);
      autofill_suggestion.frontend_id = suggestion.identifier;
      autofill_suggestion.payload = autofill::Suggestion::BackendId();
      _popupDelegate->DidAcceptSuggestion(autofill_suggestion, 0);
    }
    return;
  }

  web::WebFrame* frame =
      web::GetWebFrameWithId(_webState, SysNSStringToUTF8(frameID));
  if (!frame) {
    // The frame no longer exists, so the field can not be filled.
    if (_suggestionHandledCompletion) {
      SuggestionHandledCompletion suggestionHandledCompletionCopy =
          [_suggestionHandledCompletion copy];
      _suggestionHandledCompletion = nil;
      suggestionHandledCompletionCopy();
    }
    return;
  }

  if (suggestion.identifier == autofill::POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY) {
    // FormSuggestion is a simple, single value that can be filled out now.
    [self fillField:SysNSStringToUTF8(fieldIdentifier)
        uniqueFieldID:uniqueFieldID
             formName:SysNSStringToUTF8(formName)
                value:SysNSStringToUTF16(suggestion.value)
              inFrame:frame];
  } else if (suggestion.identifier == autofill::POPUP_ITEM_ID_CLEAR_FORM) {
    __weak AutofillAgent* weakSelf = self;
    SuggestionHandledCompletion suggestionHandledCompletionCopy =
        [_suggestionHandledCompletion copy];
    _suggestionHandledCompletion = nil;
    autofill::AutofillJavaScriptFeature::GetInstance()
        ->ClearAutofilledFieldsForForm(
            frame, uniqueFormID, uniqueFieldID,
            base::BindOnce(^(NSString* jsonString) {
              AutofillAgent* strongSelf = weakSelf;
              if (!strongSelf)
                return;
              [strongSelf updateFieldManagerForClearedIDs:jsonString];
              suggestionHandledCompletionCopy();
            }));

  } else if (suggestion.identifier ==
             autofill::POPUP_ITEM_ID_SHOW_ACCOUNT_CARDS) {
    autofill::BrowserAutofillManager* autofillManager =
        [self autofillManagerFromWebState:_webState webFrame:frame];
    if (autofillManager) {
      autofillManager->OnUserAcceptedCardsFromAccountOption();
    }
  } else {
    NOTREACHED() << "unknown identifier " << suggestion.identifier;
  }
}

- (SuggestionProviderType)type {
  return SuggestionProviderTypeAutofill;
}

- (autofill::PopupType)suggestionType {
  return _popupDelegate ? _popupDelegate->GetPopupType()
                        : autofill::PopupType::kUnspecified;
}

#pragma mark - AutofillDriverIOSBridge

- (void)fillFormData:(const autofill::FormData&)form
             inFrame:(web::WebFrame*)frame {
  base::Value::Dict autofillData;
  autofillData.Set("formName", base::Value(base::UTF16ToUTF8(form.name)));
  autofillData.Set(
      "formRendererID",
      base::Value(static_cast<int>(form.unique_renderer_id.value())));

  base::Value::Dict fieldsData;
  for (const auto& field : form.fields) {
    // Skip empty fields and those that are not autofilled.
    if (field.value.empty() || !field.is_autofilled)
      continue;

    base::Value::Dict fieldData;
    fieldData.Set("value", field.value);
    fieldData.Set("section", field.section.ToString());
    fieldsData.Set(NumberToString(field.unique_renderer_id.value()),
                   std::move(fieldData));
  }
  autofillData.Set("fields", std::move(fieldsData));

  // Store the form data when WebState is not visible, to send it as soon as it
  // becomes visible again, e.g., when the CVC unmask prompt is showing.
  if (!_webState->IsVisible()) {
    _pendingFormData =
        std::make_pair(web::GetWebFrameId(frame), std::move(autofillData));
  } else {
    [self sendData:std::move(autofillData) toFrame:frame];
  }

  autofill::BrowserAutofillManager* autofillManager =
      [self autofillManagerFromWebState:_webState webFrame:frame];
  if (autofillManager)
    autofillManager->OnDidFillAutofillFormData(
        form, autofill::AutofillTickClock::NowTicks());
}

- (void)handleParsedForms:(const std::vector<autofill::FormStructure*>&)forms
                  inFrame:(web::WebFrame*)frame {
  // No op.
}

- (void)fillFormDataPredictions:
            (const std::vector<autofill::FormDataPredictions>&)forms
                        inFrame:(web::WebFrame*)frame {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillShowTypePredictions)) {
    return;
  }

  base::Value::Dict predictionData;
  for (const auto& form : forms) {
    base::Value::Dict fieldData;
    DCHECK(form.fields.size() == form.data.fields.size());
    for (size_t i = 0; i < form.fields.size(); i++) {
      fieldData.Set(
          NumberToString(form.data.fields[i].unique_renderer_id.value()),
          base::Value(form.fields[i].overall_type));
    }
    predictionData.Set(base::UTF16ToUTF8(form.data.name), std::move(fieldData));
  }
  autofill::AutofillJavaScriptFeature::GetInstance()->FillPredictionData(
      frame, std::move(predictionData));
}

#pragma mark - AutofillClientIOSBridge

- (void)showAutofillPopup:
            (const std::vector<autofill::Suggestion>&)popup_suggestions
            popupDelegate:
                (const base::WeakPtr<autofill::AutofillPopupDelegate>&)
                    delegate {
  // Convert the suggestions into an NSArray for the keyboard.
  NSMutableArray<FormSuggestion*>* suggestions = [[NSMutableArray alloc] init];
  for (auto popup_suggestion : popup_suggestions) {
    // In the Chromium implementation the identifiers represent rows on the
    // drop down of options. These include elements that aren't relevant to us
    // such as separators ... see blink::WebAutofillClient::MenuItemIDSeparator
    // for example. We can't include that enum because it's from WebKit, but
    // fortunately almost all the entries we are interested in (profile or
    // autofill entries) are zero or positive. Negative entries we are
    // interested in is autofill::POPUP_ITEM_ID_CLEAR_FORM, used to show the
    // "clear form" button.
    NSString* value = nil;
    NSString* displayDescription = nil;
    if (popup_suggestion.frontend_id >= 0) {
      // Filter out any key/value suggestions if the user hasn't typed yet.
      if (popup_suggestion.frontend_id ==
              autofill::POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY &&
          [_typedValue length] == 0) {
        continue;
      }
      // Value will contain the text to be filled in the selected element while
      // displayDescription will contain a summary of the data to be filled in
      // the other elements.
      value = SysUTF16ToNSString(popup_suggestion.main_text.value);
      if (!popup_suggestion.labels.empty()) {
        DCHECK_EQ(popup_suggestion.labels.size(), 1U);
        DCHECK_EQ(popup_suggestion.labels[0].size(), 1U);
        displayDescription =
            SysUTF16ToNSString(popup_suggestion.labels[0][0].value);
      }
    } else if (popup_suggestion.frontend_id ==
               autofill::POPUP_ITEM_ID_CLEAR_FORM) {
      // Show the "clear form" button.
      value = SysUTF16ToNSString(popup_suggestion.main_text.value);
    } else if (popup_suggestion.frontend_id ==
               autofill::POPUP_ITEM_ID_SHOW_ACCOUNT_CARDS) {
      // Show opt-in for showing cards from account.
      value = SysUTF16ToNSString(popup_suggestion.main_text.value);
    }

    if (!value)
      continue;

    NSString* acceptanceA11yAnnouncement =
        popup_suggestion.acceptance_a11y_announcement.has_value()
            ? SysUTF16ToNSString(*popup_suggestion.acceptance_a11y_announcement)
            : nil;
    // Only show icon for credit card suggestions.
    NSString* icon = delegate && delegate->GetPopupType() ==
                                     autofill::PopupType::kCreditCards
                         ? base::SysUTF8ToNSString(popup_suggestion.icon)
                         : nil;
    FormSuggestion* suggestion =
        [FormSuggestion suggestionWithValue:value
                         displayDescription:displayDescription
                                       icon:icon
                                 identifier:popup_suggestion.frontend_id
                             requiresReauth:NO
                 acceptanceA11yAnnouncement:acceptanceA11yAnnouncement];

    // Put "clear form" entry at the front of the suggestions.
    if (popup_suggestion.frontend_id == autofill::POPUP_ITEM_ID_CLEAR_FORM) {
      [suggestions insertObject:suggestion atIndex:0];
    } else {
      [suggestions addObject:suggestion];
    }
  }

  [self onSuggestionsReady:suggestions popupDelegate:delegate];

  if (delegate)
    delegate->OnPopupShown();
}

- (void)hideAutofillPopup {
  [self onSuggestionsReady:@[]
             popupDelegate:base::WeakPtr<autofill::AutofillPopupDelegate>()];
}

- (bool)isLastQueriedField:(autofill::FieldGlobalId)fieldID {
  return fieldID == _lastQueriedFieldID;
}

#pragma mark - CRWWebStateObserver

- (void)webStateWasShown:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (!_pendingFormData.has_value()) {
    return;
  }

  std::pair<std::string, base::Value::Dict> pendingFormData =
      std::move(_pendingFormData).value();
  _pendingFormData = absl::nullopt;

  // The frameID cannot be empty.
  DCHECK(!pendingFormData.first.empty());
  web::WebFrame* frame =
      web::GetWebFrameWithId(_webState, pendingFormData.first);
  [self sendData:std::move(pendingFormData.second) toFrame:frame];
}

- (void)webState:(web::WebState*)webState
    frameDidBecomeAvailable:(web::WebFrame*)web_frame {
  DCHECK_EQ(_webState, webState);
  DCHECK(web_frame);
  if (![self isAutofillEnabled] || webState->IsLoading()) {
    return;
  }
  if (web_frame->IsMainFrame()) {
    [self processPage:webState];
    return;
  }
  // Check that the main frame has already been processed.
  if (!webState->GetWebFramesManager()->GetMainWebFrame()) {
    return;
  }
  if (!autofill::AutofillDriverIOS::FromWebStateAndWebFrame(
           webState, webState->GetWebFramesManager()->GetMainWebFrame())
           ->is_processed()) {
    return;
  }
  [self processFrame:web_frame inWebState:webState];
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  if (![self isAutofillEnabled])
    return;

  [self processPage:webState];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _last_submitted_autofill_driver = nullptr;
  if (_webState) {
    _formActivityObserverBridge.reset();
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
  }
  // Do not wait for deallocation. Remove all observers here.
  _prefChangeRegistrar.RemoveAll();
}

#pragma mark - Private methods

- (void)processPage:(web::WebState*)webState {
  web::WebFramesManager* framesManager = webState->GetWebFramesManager();
  if (!framesManager->GetMainWebFrame()) {
    return;
  }
  [self processFrame:framesManager->GetMainWebFrame() inWebState:webState];
  for (auto* frame : framesManager->GetAllWebFrames()) {
    if (frame->IsMainFrame()) {
      continue;
    }
    [self processFrame:frame inWebState:webState];
  }
}

- (void)processFrame:(web::WebFrame*)frame inWebState:(web::WebState*)webState {
  if (!frame || !frame->CanCallJavaScriptFunction())
    return;

  autofill::AutofillDriverIOS* driver =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(webState, frame);
  // This process is only done once.
  if (driver->is_processed())
    return;
  driver->set_processed(true);
  autofill::AutofillJavaScriptFeature* autofill_feature =
      autofill::AutofillJavaScriptFeature::GetInstance();
  autofill_feature->AddJSDelayInFrame(frame);

  if (frame->IsMainFrame()) {
    _popupDelegate.reset();
    _suggestionsAvailableCompletion = nil;
    _suggestionHandledCompletion = nil;
    _mostRecentSuggestions = nil;
    _typedValue = nil;
  }

  autofill::FormHandlersJavaScriptFeature* formHandlerFeature =
      autofill::FormHandlersJavaScriptFeature::GetInstance();

  // Use a delay of 200ms when tracking form mutations to reduce the
  // communication overhead (as mutations are likely to come in batch).
  constexpr int kMutationTrackingEnabledDelayInMs = 200;
  formHandlerFeature->TrackFormMutations(frame,
                                         kMutationTrackingEnabledDelayInMs);

  formHandlerFeature->ToggleTrackingUserEditedFields(
      frame,
      /*track_user_edited_fields=*/true);

  [self scanFormsInWebState:webState inFrame:frame];
}

- (void)scanFormsInWebState:(web::WebState*)webState
                    inFrame:(web::WebFrame*)webFrame {
  __weak AutofillAgent* weakSelf = self;
  id completionHandler = ^(BOOL success, const FormDataVector& forms) {
    AutofillAgent* strongSelf = weakSelf;
    if (!strongSelf || !success)
      return;
    autofill::BrowserAutofillManager* autofillManager =
        [strongSelf autofillManagerFromWebState:webState webFrame:webFrame];
    if (!autofillManager || forms.empty())
      return;
    [strongSelf notifyBrowserAutofillManager:autofillManager ofFormsSeen:forms];
  };
  // The document has now been fully loaded. Scan for forms to be extracted.
  size_t min_required_fields =
      MIN(autofill::kMinRequiredFieldsForUpload,
          MIN(autofill::kMinRequiredFieldsForHeuristics,
              autofill::kMinRequiredFieldsForQuery));
  [self fetchFormsFiltered:NO
                        withName:std::u16string()
      minimumRequiredFieldsCount:min_required_fields
                         inFrame:webFrame
               completionHandler:completionHandler];
}

#pragma mark -
#pragma mark FormActivityObserver

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  DCHECK_EQ(_webState, webState);
  if (![self isAutofillEnabled])
    return;

  if (!frame || !frame->CanCallJavaScriptFunction())
    return;

  // Return early if the page is not processed yet.
  DCHECK(autofill::AutofillDriverIOS::FromWebStateAndWebFrame(webState, frame));
  if (!autofill::AutofillDriverIOS::FromWebStateAndWebFrame(webState, frame)
           ->is_processed())
    return;

  // Return early if |params| is not complete.
  if (params.input_missing)
    return;

  // If the event is a form_changed, then the event concerns the whole page and
  // not a particular form. The whole page need to be reparsed to find the new
  // forms.
  if (params.type == "form_changed") {
    [self scanFormsInWebState:webState inFrame:frame];
    return;
  }

  // We are only interested in 'input' events in order to notify the autofill
  // manager for metrics purposes.
  if (params.type != "input" ||
      (params.field_type != "text" && params.field_type != "password")) {
    return;
  }

  // The completion block is executed asynchronously, thus it cannot refer
  // directly to `params.field_identifier` (as params is passed by reference
  // and may have been destroyed by the point the block is executed) nor to
  // web::WebFrame* (as it can be deallocated before the block execution).
  //
  // Copy the `field_identifier` to a local variable that can be captured
  // and save the frame identifier that will be used to get the WebFrame in
  // -onFormsFetched:formsData:webFrameId:fieldIdentifier.
  __weak AutofillAgent* weakSelf = self;
  __block const std::string webFrameId = frame->GetFrameId();
  __block FieldRendererId fieldIdentifier = params.unique_field_id;
  auto completionHandler = ^(BOOL success, const FormDataVector& forms) {
    [weakSelf onFormsFetched:success
                   formsData:forms
                  webFrameId:webFrameId
             fieldIdentifier:fieldIdentifier];
  };

  // Extract the active form and field only. There is no minimum field
  // requirement because key/value suggestions are offered even on short forms.
  [self fetchFormsFiltered:YES
                        withName:base::UTF8ToUTF16(params.form_name)
      minimumRequiredFieldsCount:1
                         inFrame:frame
               completionHandler:completionHandler];
}

- (void)webState:(web::WebState*)webState
    didSubmitDocumentWithFormNamed:(const std::string&)formName
                          withData:(const std::string&)formData
                    hasUserGesture:(BOOL)hasUserGesture
                           inFrame:(web::WebFrame*)frame {
  if (![self isAutofillEnabled])
    return;
  if (!frame || !frame->CanCallJavaScriptFunction())
    return;
  FormDataVector forms;

  bool success = autofill::ExtractFormsData(
      base::SysUTF8ToNSString(formData), true, base::UTF8ToUTF16(formName),
      webState->GetLastCommittedURL(), frame->GetSecurityOrigin(), &forms);

  autofill::BrowserAutofillManager* autofillManager =
      [self autofillManagerFromWebState:webState webFrame:frame];
  if (!autofillManager || !success || forms.empty())
    return;
  // AutofillDriverIOSWebFrame keeps a refcountable AutofillDriverIOS. This is a
  // workaround crbug.com/892612. On submission, AutofillDownloadManager starts
  // asynchronous tasks, which would be cancelled immediately if the
  // BrowserAutofillManager (which owns AutofillDownloadManager) was destroyed
  // immediately. For that reason, AutofillAgent keeps the latest submitted
  // AutofillDriver alive.
  // TODO(crbug.com/892612): remove this workaround once life cycle of
  // AutofillDownloadManager is fixed.
  DCHECK(frame);
  _last_submitted_autofill_driver =
      autofill::AutofillDriverIOSWebFrame::FromWebFrame(frame)
          ->GetRetainableDriver();
  DCHECK(_last_submitted_autofill_driver);
  DCHECK(forms.size() <= 1) << "Only one form should be extracted.";
  [self notifyBrowserAutofillManager:autofillManager
                    ofFormsSubmitted:forms
                       userInitiated:hasUserGesture];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  // Processing the page can be needed here if Autofill is enabled in settings
  // when the page is already loaded.
  if ([self isAutofillEnabled])
    [self processPage:_webState];
}

#pragma mark - Private methods

// Returns whether Autofill is enabled by checking if Autofill is turned on and
// if the current URL has a web scheme and the page content is HTML.
- (BOOL)isAutofillEnabled {
  if (!autofill::prefs::IsAutofillProfileEnabled(_prefService) &&
      !autofill::prefs::IsAutofillCreditCardEnabled(_prefService)) {
    return NO;
  }

  // Only web URLs are supported by Autofill.
  return web::UrlHasWebScheme(_webState->GetLastCommittedURL()) &&
         _webState->ContentIsHTML();
}

// Complete a field identified with |fieldIdentifier| on the form named
// |formName| in |frame| using |value| then move the cursor.
// TODO(crbug.com/661621): |dataString| ends up at fillFormField() in
// autofill_controller.js. fillFormField() expects an AutofillFormFieldData
// object, which |dataString| is not because 'form' is not a specified member of
// AutofillFormFieldData. fillFormField() also expects members 'max_length' and
// 'is_checked' to exist.
- (void)fillField:(const std::string&)fieldIdentifier
    uniqueFieldID:(FieldRendererId)uniqueFieldID
         formName:(const std::string&)formName
            value:(const std::u16string)value
          inFrame:(web::WebFrame*)frame {
  base::Value::Dict data;
  data.Set("unique_renderer_id", static_cast<int>(uniqueFieldID.value()));
  data.Set("identifier", fieldIdentifier);
  data.Set("form", formName);
  data.Set("value", value);

  DCHECK(_suggestionHandledCompletion);
  __weak AutofillAgent* weakSelf = self;
  SuggestionHandledCompletion suggestionHandledCompletionCopy =
      [_suggestionHandledCompletion copy];
  _suggestionHandledCompletion = nil;
  autofill::AutofillJavaScriptFeature::GetInstance()->FillActiveFormField(
      frame, std::move(data), base::BindOnce(^(BOOL success) {
        AutofillAgent* strongSelf = weakSelf;
        if (!strongSelf)
          return;
        if (success) {
          strongSelf->_fieldDataManager->UpdateFieldDataMap(
              uniqueFieldID, value, kAutofilledOnUserTrigger);
        }
        suggestionHandledCompletionCopy();
      }));
}

- (void)updateFieldManagerWithFillingResults:(NSString*)jsonString {
  std::map<uint32_t, std::u16string> fillingResults;
  if (autofill::ExtractFillingResults(jsonString, &fillingResults)) {
    for (auto& fillData : fillingResults) {
      _fieldDataManager->UpdateFieldDataMap(FieldRendererId(fillData.first),
                                            fillData.second,
                                            kAutofilledOnUserTrigger);
    }
  }
  // TODO(crbug/1131038): Remove once the experiment is over.
  UMA_HISTOGRAM_BOOLEAN("Autofill.FormFillSuccessIOS", !fillingResults.empty());

  ukm::SourceId source_id = ukm::GetSourceIdForWebStateDocument(_webState);
  ukm::builders::Autofill_FormFillSuccessIOS(source_id)
      .SetFormFillSuccess(!fillingResults.empty())
      .Record(ukm::UkmRecorder::Get());
}

// Sends the the |data| to |frame| to actually fill the data.
- (void)sendData:(base::Value::Dict)data toFrame:(web::WebFrame*)frame {
  DCHECK(_webState->IsVisible());
  __weak AutofillAgent* weakSelf = self;
  SuggestionHandledCompletion suggestionHandledCompletionCopy =
      [_suggestionHandledCompletion copy];
  _suggestionHandledCompletion = nil;
  autofill::AutofillJavaScriptFeature::GetInstance()->FillForm(
      frame, std::move(data), _pendingAutocompleteFieldID,
      base::BindOnce(^(NSString* jsonString) {
        AutofillAgent* strongSelf = weakSelf;
        if (!strongSelf)
          return;
        [strongSelf updateFieldManagerWithFillingResults:jsonString];
        // It is possible that the fill was not initiated by selecting
        // a suggestion in this case the callback is nil.
        if (suggestionHandledCompletionCopy)
          suggestionHandledCompletionCopy();
      }));
}

// Helper method used to implement the aynchronous completion block of
// -webState:didRegisterFormActivity:inFrame:. Due to the asynchronous
// invocation, WebState* and WebFrame* may both have been destroyed, so
// the method needs to check for those edge cases.
- (void)onFormsFetched:(BOOL)success
             formsData:(const FormDataVector&)forms
            webFrameId:(const std::string&)webFrameId
       fieldIdentifier:(FieldRendererId)fieldIdentifier {
  if (!success || forms.size() != 1)
    return;

  if (!_webState)
    return;

  DCHECK(_webState->GetWebFramesManager());
  web::WebFrame* webFrame =
      _webState->GetWebFramesManager()->GetFrameWithId(webFrameId);
  if (!webFrame)
    return;

  autofill::BrowserAutofillManager* autofillManager =
      [self autofillManagerFromWebState:_webState webFrame:webFrame];
  if (!autofillManager)
    return;

  autofill::FormFieldData field;
  GetFormField(&field, forms[0], fieldIdentifier);
  autofillManager->OnTextFieldDidChange(
      forms[0], field, gfx::RectF(), autofill::AutofillTickClock::NowTicks());
}

@end
