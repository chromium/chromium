// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_agent.h"

#import <UIKit/UIKit.h>

#import <cstdint>
#import <memory>
#import <optional>
#import <string>
#import <tuple>
#import <utility>

#import "base/apple/foundation_util.h"
#import "base/containers/map_util.h"
#import "base/debug/crash_logging.h"
#import "base/feature_list.h"
#import "base/format_macros.h"
#import "base/functional/bind.h"
#import "base/json/json_reader.h"
#import "base/json/json_writer.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/field_trial.h"
#import "base/metrics/histogram_functions.h"
#import "base/ranges/algorithm.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "base/types/cxx23_to_underlying.h"
#import "base/uuid.h"
#import "base/values.h"
#import "build/branding_buildflags.h"
#import "components/autofill/core/browser/autofill_field.h"
#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/filling_product.h"
#import "components/autofill/core/browser/metrics/autofill_metrics.h"
#import "components/autofill/core/browser/ui/suggestion.h"
#import "components/autofill/core/browser/ui/suggestion_type.h"
#import "components/autofill/core/common/autofill_constants.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/core/common/autofill_util.h"
#import "components/autofill/core/common/field_data_manager.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/form_data_predictions.h"
#import "components/autofill/core/common/form_field_data.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/browser/password_autofill_agent.h"
#import "components/autofill/ios/common/features.h"
#import "components/autofill/ios/common/field_data_manager_factory_ios.h"
#import "components/autofill/ios/form_util/autofill_form_features_java_script_feature.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/grit/components_resources.h"
#import "components/plus_addresses/features.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/js_messaging/web_frames_manager_observer_bridge.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "third_party/abseil-cpp/absl/types/variant.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/gfx/geometry/rect.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

using autofill::AutofillFormFeaturesJavaScriptFeature;
using autofill::AutofillJavaScriptFeature;
using autofill::FieldDataManager;
using autofill::FieldDataManagerFactoryIOS;
using autofill::FieldGlobalId;
using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormGlobalId;
using autofill::FormHandlersJavaScriptFeature;
using autofill::FormRendererId;
using autofill::FieldPropertiesFlags::kAutofilledOnUserTrigger;
using base::NumberToString;
using base::SysNSStringToUTF16;
using base::SysNSStringToUTF8;
using base::SysUTF16ToNSString;
using base::SysUTF8ToNSString;

namespace {

using FormDataVector = std::vector<FormData>;
// Maps each field id to their respective host form id. This is needed as the
// information linking the fields to their host form is lost between the moment
// of filling and when receiving the filling response.
using FieldToFormLookupMap = std::map<FieldRendererId, FormRendererId>;

// Contains the data for doing filling.
struct AutofillData {
  std::string frameID;
  base::Value::Dict payload;
  FieldToFormLookupMap fieldToFormLookupMap;
};

// The type of the completion handler callback for
// |fetchFormsWithName:completionHandler|
using FetchFormsCompletionHandler =
    base::OnceCallback<void(BOOL, const FormDataVector&)>;

// Delay for setting an utterance to be queued, it is required to ensure that
// standard announcements have already been started and thus would not interrupt
// the enqueued utterance.
constexpr base::TimeDelta kA11yAnnouncementQueueDelay = base::Seconds(1);

// The correct icon size to use in suggestions. Used to ensure images are scaled
// appropriately.
constexpr CGFloat kSuggestionIconWidth = 32;

bool ContainsFocusableField(const FormData& form, FieldRendererId field_id) {
  auto it =
      base::ranges::find(form.fields(), field_id, &FormFieldData::renderer_id);
  return it != form.fields().end() && it->is_focusable();
}

}  // namespace

@interface AutofillAgent () <CRWWebStateObserver,
                             CRWWebFramesManagerObserver,
                             FormActivityObserver,
                             PrefObserverDelegate> {
  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  raw_ptr<web::WebState> _webState;

  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe the web frames manager from Objective-C.
  std::unique_ptr<web::WebFramesManagerObserverBridge>
      _webFramesManagerObserverBridge;

  // The pref service for which this agent was created.
  raw_ptr<PrefService> _prefService;

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

  // Delegate for the most recent suggestions.
  // The reference is weak because a weak pointer is sent to our
  // BrowserAutofillManagerDelegate.
  base::WeakPtr<autofill::AutofillSuggestionDelegate> _suggestionDelegate;

  // The autofill data that needs to be sent when the |webState_| is shown.
  std::optional<AutofillData> _pendingFormData;

  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // Bridge to observe form activity in |webState_|.
  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;

  // ID of the last Autofill query made. Used to discard outdated suggestions.
  FieldGlobalId _lastQueriedFieldID;
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
    _webFramesManagerObserverBridge =
        std::make_unique<web::WebFramesManagerObserverBridge>(self);
    web::WebFramesManager* framesManager =
        AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
            _webState);
    framesManager->AddObserver(_webFramesManagerObserverBridge.get());
    _formActivityObserverBridge =
        std::make_unique<autofill::FormActivityObserverBridge>(_webState, self);
    _prefService = prefService;
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefChangeRegistrar.Init(prefService);
    _prefObserverBridge->ObserveChangesForPreference(
        autofill::prefs::kAutofillCreditCardEnabled, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        autofill::prefs::kAutofillProfileEnabled, &_prefChangeRegistrar);
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    [self webStateDestroyed:_webState];
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

  if (![self isAutofillEnabled]) {
    completion(NO);
    return;
  }

  // Check for suggestions if the form activity is initiated by the user.
  if (!hasUserGesture) {
    completion(NO);
    return;
  }

  web::WebFramesManager* frames_manager =
      AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(_webState);
  web::WebFrame* frame =
      frames_manager->GetFrameWithId(SysNSStringToUTF8(formQuery.frameID));
  if (!frame) {
    completion(NO);
    return;
  }

  const auto callback = [](AutofillAgent* agent,
                           FormSuggestionProviderQuery* formQuery,
                           base::WeakPtr<web::WebFrame> frame,
                           base::WeakPtr<web::WebState> webState,
                           SuggestionsAvailableCompletion completion,
                           BOOL success, const FormDataVector& forms) {
    if (success && forms.size() == 1) {
      // Once the active form and field are extracted, send a query to the
      // BrowserAutofillManager for suggestions.
      [agent queryAutofillForForm:forms[0]
                  fieldIdentifier:formQuery.fieldRendererID
                             type:formQuery.type
                       typedValue:formQuery.typedValue
                            frame:frame
                         webState:webState
                completionHandler:completion];
    }
  };

  // Re-extract the active form and field only. All forms with at least one
  // input element are considered because key/value suggestions are offered
  // even on short forms.
  [self fetchFormsFiltered:YES
                  withName:SysNSStringToUTF16(formQuery.formName)
                   inFrame:frame
         completionHandler:base::BindOnce(callback, self, formQuery,
                                          frame->AsWeakPtr(),
                                          webState->GetWeakPtr(), completion)];
}

- (void)retrieveSuggestionsForForm:(FormSuggestionProviderQuery*)formQuery
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  DCHECK(_mostRecentSuggestions) << "Requestor should have called "
                                 << "|checkIfSuggestionsAvailableForForm:"
                                    "webState:completionHandler:|.";
  completion(_mostRecentSuggestions, self);
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                    atIndex:(NSInteger)index
                       form:(NSString*)formName
             formRendererID:(FormRendererId)formRendererID
            fieldIdentifier:(NSString*)fieldIdentifier
            fieldRendererID:(FieldRendererId)fieldRendererID
                    frameID:(NSString*)frameID
          completionHandler:(SuggestionHandledCompletion)completion {
  [[UIDevice currentDevice] playInputClick];
  DCHECK(completion);
  // TODO(crbug.com/366247033): This double-checks the assumption that this
  // crash is caused by an unexpected suggestion type, and not a nil suggestion.
  // It can be removed once a root cause for the issue is known.
  CHECK(suggestion, base::NotFatalUntil::M133);

  _suggestionHandledCompletion = [completion copy];

  if (suggestion.acceptanceA11yAnnouncement != nil) {
    // The announcement is done asyncronously with certain delay to make sure
    // it is not interrupted by (almost) immediate standard announcements.
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW,
                      kA11yAnnouncementQueueDelay.InNanoseconds()),
        dispatch_get_main_queue(), ^{
          // Queueing flag allows to preserve standard announcements,
          // they are conveyed first and then announce this message.
          // This is a tradeoff as there is no control over the
          // standard utterances (they are interrupting) and it is
          // not desirable to interrupt them. Hence acceptance
          // announcement is done after standard ones (which takes
          // seconds).
          NSAttributedString* message = [[NSAttributedString alloc]
              initWithString:suggestion.acceptanceA11yAnnouncement
                  attributes:@{
                    UIAccessibilitySpeechAttributeQueueAnnouncement : @YES
                  }];
          UIAccessibilityPostNotification(
              UIAccessibilityAnnouncementNotification, message);
        });
  }

  if (suggestion.type == autofill::SuggestionType::kAddressEntry ||
      suggestion.type == autofill::SuggestionType::kCreditCardEntry ||
      suggestion.type == autofill::SuggestionType::kCreateNewPlusAddress ||
      suggestion.type == autofill::SuggestionType::kVirtualCreditCardEntry ||
      ((base::FeatureList::IsEnabled(
            autofill::features::kAutofillAddressFieldSwapping) &&
        suggestion.type ==
            autofill::SuggestionType::kAddressFieldByFieldFilling))) {
    _pendingAutocompleteFieldID = fieldRendererID;
    if (_suggestionDelegate) {
      autofill::Suggestion autofill_suggestion;
      autofill_suggestion.main_text.value =
          SysNSStringToUTF16(suggestion.value);
      autofill_suggestion.type = suggestion.type;
      autofill_suggestion.field_by_field_filling_type_used =
          suggestion.fieldByFieldFillingTypeUsed;
      if (!suggestion.backendIdentifier.length) {
        autofill_suggestion.payload = autofill::Suggestion::BackendId();
      } else {
        autofill_suggestion.payload =
            autofill::Suggestion::BackendId(autofill::Suggestion::Guid(
                SysNSStringToUTF8(suggestion.backendIdentifier)));
      }

      _suggestionDelegate->DidAcceptSuggestion(autofill_suggestion,
                                               {static_cast<int>(index), 0});
    }
    return;
  }

  web::WebFramesManager* frames_manager =
      AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(_webState);
  web::WebFrame* frame =
      frames_manager->GetFrameWithId(SysNSStringToUTF8(frameID));
  if (!frame) {
    // The frame no longer exists, so the field can not be filled.
    if (SuggestionHandledCompletion c =
            std::exchange(_suggestionHandledCompletion, nil)) {
      c();
    }
    return;
  }

  if (suggestion.type == autofill::SuggestionType::kAutocompleteEntry ||
      suggestion.type == autofill::SuggestionType::kFillExistingPlusAddress) {
    // FormSuggestion is a simple, single value that can be filled out now.
    [self fillField:SysNSStringToUTF8(fieldIdentifier)
        fieldRendererID:fieldRendererID
         formRendererID:formRendererID
               formName:SysNSStringToUTF8(formName)
                  value:SysNSStringToUTF16(suggestion.value)
                inFrame:frame];
  } else if (suggestion.type == autofill::SuggestionType::kUndoOrClear) {
    const auto callback = [](__weak AutofillAgent* agent,
                             base::WeakPtr<web::WebFrame> frame,
                             FormRendererId formId,
                             SuggestionHandledCompletion completion,
                             NSString* jsonString) {
      if (frame) {
        [agent onDidClearFields:jsonString inFrame:frame.get() inForm:formId];
      }
      // Only run the completion if set as it isn't impossible that the provided
      // completion is nil.
      if (completion) {
        completion();
      }
    };

    __weak __typeof(self) weakSelf = self;
    AutofillJavaScriptFeature::GetInstance()->ClearAutofilledFieldsForForm(
        frame, formRendererID, fieldRendererID,
        base::BindOnce(callback, weakSelf, frame->AsWeakPtr(), formRendererID,
                       std::exchange(_suggestionHandledCompletion, nil)));

  } else if (suggestion.type == autofill::SuggestionType::kShowAccountCards) {
    autofill::BrowserAutofillManager* autofillManager =
        [self autofillManagerFromWebState:_webState webFrame:frame];
    if (autofillManager) {
      autofillManager->OnUserAcceptedCardsFromAccountOption();
    }
  } else {
    // TODO(crbug.com/366247033): Remove this crash key once the underlying
    // crash has been fixed.
    SCOPED_CRASH_KEY_NUMBER("Bug366247033", "suggestion_type",
                            static_cast<int>(suggestion.type));
    NOTREACHED(base::NotFatalUntil::M133);
  }
}

- (SuggestionProviderType)type {
  return SuggestionProviderTypeAutofill;
}

- (autofill::FillingProduct)mainFillingProduct {
  return _suggestionDelegate ? _suggestionDelegate->GetMainFillingProduct()
                             : autofill::FillingProduct::kNone;
}

#pragma mark - AutofillDriverIOSBridge

- (void)fillData:(const std::vector<autofill::FormFieldData::FillData>&)data
         inFrame:(web::WebFrame*)frame {
  base::Value::Dict fieldsData;
  FieldToFormLookupMap fieldToFormLookupMap;

  for (const auto& field : data) {
    // Skip empty fields and those that are not autofilled.
    if (field.value.empty() || !field.is_autofilled) {
      continue;
    }

    base::Value::Dict fieldData;
    fieldData.Set("value", field.value);
    fieldData.Set("section", field.section.ToString());
    fieldData.Set("hostFormId", static_cast<int>(*field.host_form_id));
    fieldsData.Set(NumberToString(*field.renderer_id), std::move(fieldData));

    fieldToFormLookupMap[field.renderer_id] = field.host_form_id;
  }
  auto payload = base::Value::Dict().Set("fields", std::move(fieldsData));
  AutofillData autofillData = {
      .frameID = frame ? frame->GetFrameId() : "",
      .payload = std::move(payload),
      .fieldToFormLookupMap = std::move(fieldToFormLookupMap)};

  // Store the form data when WebState is not visible, to send it as soon as it
  // becomes visible again, e.g., when the CVC unmask prompt is showing.
  if (!_webState->IsVisible()) {
    _pendingFormData = std::move(autofillData);
  } else {
    [self sendData:std::move(autofillData) toFrame:frame];
  }
}

// Similar to `fillField`, but does not rely on `FillActiveFormField`, opting
// instead to find and fill a specific field in `frame` with `value`. In other
// words, `field` need not be `document.activeElement`.
- (void)fillSpecificFormField:(const FieldRendererId&)field
                    withValue:(const std::u16string)value
                      inFrame:(web::WebFrame*)frame {
  base::Value::Dict data;
  data.Set("renderer_id", static_cast<int>(field.value()));
  data.Set("value", value);

  const auto callback =
      [](__weak AutofillAgent* agent, SuggestionHandledCompletion completion,
         FieldRendererId fieldId, base::WeakPtr<web::WebFrame> frame,
         const std::u16string& value, BOOL success) {
        if (success && frame) {
          [agent onDidFillField:fieldId
                           form:std::nullopt
                          frame:frame.get()
                          value:value];
        }
        // Only run the completion if set as it isn't impossible that the
        // provided completion is nil.
        if (completion) {
          completion();
        }
      };

  __weak __typeof(self) weakSelf = self;
  AutofillJavaScriptFeature::GetInstance()->FillSpecificFormField(
      frame, std::move(data),
      base::BindOnce(callback, weakSelf,
                     std::exchange(_suggestionHandledCompletion, nil), field,
                     frame->AsWeakPtr(), value));
}

- (void)handleParsedForms:
            (const std::vector<
                raw_ptr<autofill::FormStructure, VectorExperimental>>&)forms
                  inFrame:(web::WebFrame*)frame {
}

- (void)fillFormDataPredictions:
            (const std::vector<autofill::FormDataPredictions>&)forms
                        inFrame:(web::WebFrame*)frame {
  if (!base::FeatureList::IsEnabled(
          autofill::features::test::kAutofillShowTypePredictions)) {
    return;
  }

  base::Value::Dict predictionData;
  for (const auto& form : forms) {
    base::Value::Dict fieldData;
    DCHECK(form.fields.size() == form.data.fields().size());
    for (size_t i = 0; i < form.fields.size(); i++) {
      fieldData.Set(NumberToString(form.data.fields()[i].renderer_id().value()),
                    base::Value(form.fields[i].overall_type));
    }
    predictionData.Set(base::UTF16ToUTF8(form.data.name()),
                       std::move(fieldData));
  }
  AutofillJavaScriptFeature::GetInstance()->FillPredictionData(
      frame, std::move(predictionData));
}

- (void)scanFormsInWebState:(web::WebState*)webState
                    inFrame:(web::WebFrame*)webFrame {
  __weak __typeof(self) weakSelf = self;
  const auto callback = [](__weak AutofillAgent* agent,
                           base::WeakPtr<web::WebFrame> frame, BOOL success,
                           const FormDataVector& forms) {
    if (!success || forms.empty()) {
      return;
    }
    [agent notifyFormsSeen:forms inFrame:frame.get()];
  };
  // The document has now been fully loaded. Scan for forms to be extracted.
  [self fetchFormsFiltered:NO
                  withName:std::u16string()
                   inFrame:webFrame
         completionHandler:base::BindOnce(callback, weakSelf,
                                          webFrame->AsWeakPtr())];
}

#pragma mark - AutofillClientIOSBridge

- (void)showAutofillPopup:
            (const std::vector<autofill::Suggestion>&)popup_suggestions
       suggestionDelegate:
           (const base::WeakPtr<autofill::AutofillSuggestionDelegate>&)
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
    // interested in is autofill::SuggestionType::kUndoOrClear, used to show the
    // "clear form" button.
    // TODO(crbug.com/40266549): Replace Clear Form with Undo
    NSString* value = nil;
    NSString* minorValue = nil;
    NSString* displayDescription = nil;
    UIImage* icon = nil;

    if (popup_suggestion.type == autofill::SuggestionType::kAutocompleteEntry ||
        popup_suggestion.type == autofill::SuggestionType::kAddressEntry ||
        popup_suggestion.type == autofill::SuggestionType::kCreditCardEntry ||
        popup_suggestion.type ==
            autofill::SuggestionType::kVirtualCreditCardEntry ||
        (base::FeatureList::IsEnabled(
             autofill::features::kAutofillAddressFieldSwapping) &&
         popup_suggestion.type ==
             autofill::SuggestionType::kAddressFieldByFieldFilling)) {
      // Filter out any key/value suggestions if the user hasn't typed yet.
      if (popup_suggestion.type ==
              autofill::SuggestionType::kAutocompleteEntry &&
          _typedValue.length == 0) {
        continue;
      }
      // Value will contain the text to be filled in the selected element while
      // displayDescription will contain a summary of the data to be filled in
      // the other elements.
      value = SysUTF16ToNSString(popup_suggestion.main_text.value);

      if (!popup_suggestion.minor_text.value.empty()) {
        // For Virtual Cards, the main_text is just "Virtual card" so we need to
        // include the minor_text (which is the card name + last 4 digits ||
        // card holder's name) as the minorValue.
        minorValue = SysUTF16ToNSString(popup_suggestion.minor_text.value);
      }

      if (!popup_suggestion.labels.empty() &&
          !popup_suggestion.labels.front().empty()) {
        DCHECK_EQ(popup_suggestion.labels.size(), 1U);
        DCHECK_EQ(popup_suggestion.labels[0].size(), 1U);
        displayDescription =
            SysUTF16ToNSString(popup_suggestion.labels[0][0].value);
      }

      // Only show icon for credit card suggestions.
      if (delegate && delegate->GetMainFillingProduct() ==
                          autofill::FillingProduct::kCreditCard) {
        icon = [self createIcon:popup_suggestion];
      }
    } else if (popup_suggestion.type ==
               autofill::SuggestionType::kUndoOrClear) {
      // Show the "clear form" button.
      // TODO(crbug.com/40266549): Replace Clear Form with Undo once this
      // changes
      value = SysUTF16ToNSString(popup_suggestion.main_text.value);
    } else if (popup_suggestion.type ==
               autofill::SuggestionType::kShowAccountCards) {
      // Show opt-in for showing cards from account.
      value = SysUTF16ToNSString(popup_suggestion.main_text.value);
    } else if (popup_suggestion.type ==
                   autofill::SuggestionType::kFillExistingPlusAddress ||
               popup_suggestion.type ==
                   autofill::SuggestionType::kCreateNewPlusAddress) {
      // Show any plus_address suggestions.
      value = SysUTF16ToNSString(popup_suggestion.main_text.value);
      if (!popup_suggestion.labels.empty() &&
          !popup_suggestion.labels.front().empty() &&
          IsKeyboardAccessoryUpgradeEnabled()) {
        displayDescription =
            SysUTF16ToNSString(popup_suggestion.labels[0][0].value);
      }
    }

    if (!value)
      continue;

    NSString* acceptanceA11yAnnouncement =
        popup_suggestion.acceptance_a11y_announcement.has_value()
            ? SysUTF16ToNSString(*popup_suggestion.acceptance_a11y_announcement)
            : nil;

    autofill::FieldType fieldByFieldFillingTypeUsed =
        (popup_suggestion.field_by_field_filling_type_used
             ? *popup_suggestion.field_by_field_filling_type_used
             : autofill::FieldType::EMPTY_TYPE);

    FormSuggestion* suggestion = [FormSuggestion
                suggestionWithValue:value
                         minorValue:minorValue
                 displayDescription:displayDescription
                               icon:icon
                               type:popup_suggestion.type
                  backendIdentifier:SysUTF8ToNSString(
                                        popup_suggestion
                                            .GetBackendId<
                                                autofill::Suggestion::Guid>()
                                            .value())
        fieldByFieldFillingTypeUsed:fieldByFieldFillingTypeUsed
                     requiresReauth:NO
         acceptanceA11yAnnouncement:acceptanceA11yAnnouncement];

    suggestion.featureForIPH = SuggestionFeatureForIPH::kUnknown;
    if (popup_suggestion.feature_for_iph ==
        &feature_engagement::
            kIPHAutofillExternalAccountProfileSuggestionFeature) {
      suggestion.featureForIPH =
          SuggestionFeatureForIPH::kAutofillExternalAccountProfile;
    } else if (popup_suggestion.feature_for_iph ==
               &feature_engagement::kIPHPlusAddressCreateSuggestionFeature) {
      suggestion.featureForIPH = SuggestionFeatureForIPH::kPlusAddressCreation;
    }

    // Put "clear form" entry at the front of the suggestions.
    if (popup_suggestion.type == autofill::SuggestionType::kUndoOrClear) {
      [suggestions insertObject:suggestion atIndex:0];
    } else {
      [suggestions addObject:suggestion];
    }
  }

  [self onSuggestionsReady:suggestions suggestionDelegate:delegate];

  // TODO(crbug.com/363958046): Pass the actually shown suggestions instead of
  // `popup_suggestions`.
  if (delegate) {
    delegate->OnSuggestionsShown(popup_suggestions);
  }
}

- (void)hideAutofillPopup {
  [self
      onSuggestionsReady:@[]
      suggestionDelegate:base::WeakPtr<autofill::AutofillSuggestionDelegate>()];
}

- (bool)isLastQueriedField:(FieldGlobalId)fieldID {
  return fieldID == _lastQueriedFieldID;
}

#pragma mark - CRWWebStateObserver

- (void)webStateWasShown:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (!_pendingFormData) {
    return;
  }

  // The frameID cannot be empty.
  const std::string& frameID = _pendingFormData->frameID;
  CHECK(!frameID.empty());
  web::WebFramesManager* frames_manager =
      AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(_webState);
  web::WebFrame* frame = frames_manager->GetFrameWithId(frameID);
  [self sendData:std::move(*_pendingFormData) toFrame:frame];
  _pendingFormData.reset();
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  if (![self isAutofillEnabled])
    return;

  [self processPage:webState];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    _formActivityObserverBridge.reset();
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    web::WebFramesManager* framesManager =
        AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
            _webState);
    framesManager->RemoveObserver(_webFramesManagerObserverBridge.get());
    _webFramesManagerObserverBridge.reset();
    _webState = nullptr;
  }
  // Do not wait for deallocation. Remove all observers here.
  _prefChangeRegistrar.RemoveAll();
}

#pragma mark - CRWWebFramesManagerObserver

- (void)webFramesManager:(web::WebFramesManager*)webFramesManager
    frameBecameAvailable:(web::WebFrame*)webFrame {
  DCHECK(_webState);
  DCHECK(webFrame);
  if (![self isAutofillEnabled] || _webState->IsLoading()) {
    return;
  }
  if (webFrame->IsMainFrame()) {
    [self processPage:_webState];
    return;
  }
  // Check that the main frame has already been processed.
  if (!webFramesManager->GetMainWebFrame()) {
    return;
  }
  auto* main_driver = autofill::AutofillDriverIOS::FromWebStateAndWebFrame(
      _webState, webFramesManager->GetMainWebFrame());
  DLOG_IF(WARNING, !main_driver) << "No AutofillDriverIOS found for WebFrame";
  if (!main_driver || !main_driver->is_processed()) {
    return;
  }
  [self processFrame:webFrame inWebState:_webState];
}

#pragma mark - FormActivityObserver

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  DCHECK_EQ(_webState, webState);
  if (![self isAutofillEnabled])
    return;

  if (!frame) {
    return;
  }

  // Return early if the page is not processed yet.
  auto* driver =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(webState, frame);
  DLOG_IF(WARNING, !driver) << "No AutofillDriverIOS found for WebFrame";
  if (!driver || !driver->is_processed()) {
    return;
  }

  // Return early if |params| is not complete.
  if (params.input_missing)
    return;

  // If the event is a form_changed, then the event concerns the whole page and
  // not a particular form. The whole document's forms need to be extracted to
  // find the new forms.
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
  // and may have been destroyed by the point the block is executed).
  __weak __typeof(self) weakSelf = self;
  const auto callback =
      [](__weak AutofillAgent* agent, base::WeakPtr<web::WebFrame> frame,
         FieldRendererId fieldId, BOOL success, const FormDataVector& forms) {
        [agent onFormsFetched:success
                    formsData:forms
                     webFrame:frame
              fieldIdentifier:fieldId];
      };

  // Extract the active form and field only.
  [self
      fetchFormsFiltered:YES
                withName:base::UTF8ToUTF16(params.form_name)
                 inFrame:frame
       completionHandler:base::BindOnce(callback, weakSelf, frame->AsWeakPtr(),
                                        params.field_renderer_id)];
}

- (void)webState:(web::WebState*)webState
    didSubmitDocumentWithFormData:(const FormData&)formData
                   hasUserGesture:(BOOL)hasUserGesture
                          inFrame:(web::WebFrame*)frame {
  if (![self isAutofillEnabled] || !frame) {
    return;
  }

  auto* driver =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(webState, frame);
  if (!driver) {
    return;
  }

  driver->FormSubmitted(formData,
                        /*known_success=*/false,
                        autofill::mojom::SubmissionSource::FORM_SUBMISSION);
}

- (void)webState:(web::WebState*)webState
    didRegisterFormRemoval:(const autofill::FormRemovalParams&)params
                   inFrame:(web::WebFrame*)frame {
  CHECK_EQ(_webState, webState);
  CHECK(!params.removed_forms.empty() || !params.removed_unowned_fields.empty())
      << "Invalid params. Form removal events with missing input should have "
         "been filtered out by FormActivityTabHelper.";

  autofill::AutofillDriverIOS* autofillDriver =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(webState, frame);
  if (!autofillDriver) {
    return;
  }

  autofillDriver->FormsRemoved(params.removed_forms,
                               params.removed_unowned_fields);
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
      !autofill::prefs::IsAutofillPaymentMethodsEnabled(_prefService)) {
    return NO;
  }

  // Only web URLs are supported by Autofill.
  return web::UrlHasWebScheme(_webState->GetLastCommittedURL()) &&
         _webState->ContentIsHTML();
}

// Fills a field identified with |fieldIdentifier| on the form named
// |formName| in |frame| using |value| then move the cursor.
// TODO(crbug.com/41284261): |dataString| ends up at fillFormField() in
// autofill_controller.js. fillFormField() expects an AutofillFormFieldData
// object, which |dataString| is not because 'form' is not a specified member of
// AutofillFormFieldData. fillFormField() also expects members 'max_length' and
// 'is_checked' to exist.
- (void)fillField:(const std::string&)fieldIdentifier
    fieldRendererID:(FieldRendererId)fieldRendererID
     formRendererID:(FormRendererId)formRendererID
           formName:(const std::string&)formName
              value:(const std::u16string)value
            inFrame:(web::WebFrame*)frame {
  base::Value::Dict data;
  data.Set("renderer_id", static_cast<int>(fieldRendererID.value()));
  data.Set("identifier", fieldIdentifier);
  data.Set("form", formName);
  data.Set("value", value);

  DCHECK(_suggestionHandledCompletion);

  const auto callback = [](__weak AutofillAgent* agent,
                           SuggestionHandledCompletion completion,
                           FieldRendererId fieldId,
                           std::optional<FormRendererId> formId,
                           base::WeakPtr<web::WebFrame> frame,
                           const std::u16string& value, BOOL success) {
    if (success && frame) {
      [agent onDidFillField:fieldId form:formId frame:frame.get() value:value];
    }
    // Only run the completion if set as it isn't impossible that the provided
    // completion is nil.
    if (completion) {
      completion();
    }
  };

  __weak __typeof(self) weakSelf = self;
  AutofillJavaScriptFeature::GetInstance()->FillActiveFormField(
      frame, std::move(data),
      base::BindOnce(
          callback, weakSelf, std::exchange(_suggestionHandledCompletion, nil),
          fieldRendererID, formRendererID, frame->AsWeakPtr(), value));
}

// Called when did fill a specific field.
- (void)onDidFillField:(FieldRendererId)fieldID
                  form:(std::optional<FormRendererId>)formID
                 frame:(web::WebFrame*)frame
                 value:(const std::u16string&)value {
  [self updateFieldManagerForSpecificField:fieldID
                                   inFrame:frame
                                 withValue:value];
  [self notifyAboutValueChangeOnField:fieldID
                               inForm:formID
                                frame:frame
                            withValue:value];
}

// Called when did fill multiple fields and received results serialized in a
// JSON string.
- (void)onDidFillWithResults:(NSString*)resultsAsJsonStr
                     inFrame:(web::WebFrame*)frame
        fieldToFormLookupMap:(const FieldToFormLookupMap&)fieldToFormLookupMap {
  std::map<uint32_t, std::u16string> fillingResults;
  if (autofill::ExtractFillingResults(resultsAsJsonStr, &fillingResults)) {
    [self updateFieldManagerWithFillingResults:fillingResults inFrame:frame];
    [self notifyAboutFormFillingResults:fillingResults
                                inFrame:frame
                   fieldToFormLookupMap:fieldToFormLookupMap];
  }

  [self recordFormFillingSuccessMetrics:!fillingResults.empty()];
}

// Called when did clear fields.
- (void)onDidClearFields:(NSString*)clearedFieldsAsJsonStr
                 inFrame:(web::WebFrame*)frame
                  inForm:(FormRendererId)formID {
  const auto clearedIDs =
      autofill::ExtractIDs<FieldRendererId>(clearedFieldsAsJsonStr);
  if (!clearedIDs) {
    return;
  }

  [self updateFieldManagerForClearedIDs:*clearedIDs inFrame:frame];
  [self notifyAboutClearedFields:*clearedIDs inFrame:frame inForm:formID];
}

// Updates field managers with filling results.
- (void)updateFieldManagerWithFillingResults:
            (const std::map<uint32_t, std::u16string>&)fillingResults
                                     inFrame:(web::WebFrame*)frame {
  for (auto& fillData : fillingResults) {
    [self updateFieldManagerForSpecificField:FieldRendererId(fillData.first)
                                     inFrame:frame
                                   withValue:fillData.second];
  }
}

- (void)updateFieldManagerForSpecificField:(FieldRendererId)fieldRendererID
                                   inFrame:(web::WebFrame*)frame
                                 withValue:(const std::u16string&)value {
  FieldDataManagerFactoryIOS::FromWebFrame(frame)->UpdateFieldDataMap(
      fieldRendererID, value, kAutofilledOnUserTrigger);
}

// Updates field managers for cleared fields.
- (void)updateFieldManagerForClearedIDs:
            (const std::set<FieldRendererId>&)clearedFields
                                inFrame:(web::WebFrame*)frame {
  for (const auto fieldID : clearedFields) {
    [self updateFieldManagerForSpecificField:fieldID
                                     inFrame:frame
                                   withValue:u""];
  }
}

// Notifies the PasswordAutofillAgent that the value of a field has changed.
- (void)notifyAboutValueChangeOnField:(FieldRendererId)fieldID
                               inForm:(std::optional<FormRendererId>)formID
                                frame:(web::WebFrame*)frame
                            withValue:(const std::u16string&)value {
  CHECK(frame);

  autofill::PasswordAutofillAgent* agent =
      autofill::PasswordAutofillAgent::FromWebState(_webState);
  agent->DidFillField(frame, formID, fieldID, value);
}

// Notifies that form filling results were received.
- (void)notifyAboutFormFillingResults:
            (const std::map<uint32_t, std::u16string>&)fillingResults
                              inFrame:(web::WebFrame*)frame
                 fieldToFormLookupMap:
                     (const FieldToFormLookupMap&)fieldToFormLookupMap {
  CHECK(frame);

  for (auto& fillData : fillingResults) {
    FieldRendererId fieldID = FieldRendererId(fillData.first);
    if (const FormRendererId* formID =
            base::FindOrNull(fieldToFormLookupMap, fieldID)) {
      [self notifyAboutValueChangeOnField:fieldID
                                   inForm:*formID
                                    frame:frame
                                withValue:fillData.second];
    }
  }
}

// Notifies that fields were cleared.
- (void)notifyAboutClearedFields:(const std::set<FieldRendererId>&)clearedFields
                         inFrame:(web::WebFrame*)frame
                          inForm:(FormRendererId)formID {
  CHECK(frame);

  for (auto fieldID : clearedFields) {
    [self notifyAboutValueChangeOnField:fieldID
                                 inForm:formID
                                  frame:frame
                              withValue:u""];
  }
}

// Sends the the |data| to |frame| to actually fill the data.
- (void)sendData:(AutofillData)data toFrame:(web::WebFrame*)frame {
  DCHECK(_webState->IsVisible());
  __weak __typeof(self) weakSelf = self;
  const auto callback =
      [](__weak AutofillAgent* agent, base::WeakPtr<web::WebFrame> frame,
         SuggestionHandledCompletion completion,
         const FieldToFormLookupMap& map, NSString* jsonString) {
        if (frame) {
          [agent onDidFillWithResults:jsonString
                              inFrame:frame.get()
                 fieldToFormLookupMap:map];
        }

        // Only run the completion if set as it isn't impossible that the
        // provided completion is nil.
        if (completion) {
          completion();
        }
      };
  AutofillJavaScriptFeature::GetInstance()->FillForm(
      frame, std::move(data.payload), _pendingAutocompleteFieldID,
      base::BindOnce(callback, weakSelf, frame->AsWeakPtr(),
                     std::exchange(_suggestionHandledCompletion, nil),
                     std::move(data.fieldToFormLookupMap)));
}

// Helper method used to implement the aynchronous completion block of
// -webState:didRegisterFormActivity:inFrame:. Due to the asynchronous
// invocation, WebState* and WebFrame* may both have been destroyed, so
// the method needs to check for those edge cases.
- (void)onFormsFetched:(BOOL)success
             formsData:(const FormDataVector&)forms
              webFrame:(base::WeakPtr<web::WebFrame>)webFrame
       fieldIdentifier:(FieldRendererId)fieldIdentifier {
  if (!success || forms.size() != 1 || !_webState || !webFrame) {
    return;
  }

  auto* driver = autofill::AutofillDriverIOS::FromWebStateAndWebFrame(
      _webState, webFrame.get());
  if (!driver) {
    return;
  }
  const FormData& form = forms[0];
  if (!ContainsFocusableField(form, fieldIdentifier)) {
    return;
  }
  driver->TextFieldDidChange(form, {form.host_frame(), fieldIdentifier},
                             base::TimeTicks::Now());
}

// Helper method to create icons for payment cards.
- (UIImage*)createIcon:(autofill::Suggestion)popup_suggestion {
  // If available, the custom icon for the card is preferred over the
  // generic network icon. The network icon may also be missing, in
  // which case we do not set an icon at all.
  if (auto* custom_icon =
          absl::get_if<gfx::Image>(&popup_suggestion.custom_icon);
      custom_icon && !custom_icon->IsEmpty()) {
    UIImage* icon = custom_icon->ToUIImage();

    // On iOS, the keyboard accessory wants smaller icons than the default
    // 40x24 size, so we resize them to 32x20, if the provided icon is
    // larger than that.
    if (icon && (icon.size.width > kSuggestionIconWidth)) {
      // For a simple image resize, we can keep the same underlying image
      // and only adjust the ratio.
      CGFloat ratio = icon.size.width / kSuggestionIconWidth;
      return [UIImage imageWithCGImage:[icon CGImage]
                                 scale:icon.scale * ratio
                           orientation:icon.imageOrientation];
    }
    return icon;
  } else if (popup_suggestion.icon != autofill::Suggestion::Icon::kNoIcon) {
    const int resourceID =
        autofill::CreditCard::IconResourceId(popup_suggestion.icon);
    return ui::ResourceBundle::GetSharedInstance()
        .GetNativeImageNamed(resourceID)
        .ToUIImage();
  }
  return nil;
}

// Returns the autofill manager associated with a web::WebState instance.
// Returns nullptr if there is no autofill manager associated anymore, this can
// happen when |close| has been called on the |webState|. Also returns nullptr
// if -webStateDestroyed: has been called.
- (autofill::BrowserAutofillManager*)
    autofillManagerFromWebState:(web::WebState*)webState
                       webFrame:(web::WebFrame*)webFrame {
  if (!webState || !_webStateObserverBridge) {
    return nullptr;
  }
  auto* driver =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(webState, webFrame);
  DLOG_IF(WARNING, !driver) << "No AutofillDriverIOS found for WebFrame";
  if (!driver) {
    return nullptr;
  }
  return &driver->GetAutofillManager();
}

// Notifies the autofill manager when forms are detected on a page.
- (void)notifyFormsSeen:(const FormDataVector&)updatedForms
                inFrame:(web::WebFrame*)frame {
  auto* driver =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(_webState, frame);
  if (!driver) {
    return;
  }

  DCHECK(!updatedForms.empty());

  driver->FormsSeen(/*updated_forms=*/updatedForms, /*removed_forms=*/{});
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
                   inFrame:(web::WebFrame*)frame
         completionHandler:(FetchFormsCompletionHandler)completionHandler {
  DCHECK(completionHandler);

  // Necessary so the values can be used inside a block.
  GURL pageURL = _webState->GetLastCommittedURL();
  GURL frameOrigin =
      frame ? frame->GetSecurityOrigin() : pageURL.DeprecatedGetOriginAsURL();

  const scoped_refptr<FieldDataManager> fieldDataManager =
      FieldDataManagerFactoryIOS::GetRetainable(frame);
  const auto callback = [](FetchFormsCompletionHandler completion,
                           BOOL filtered, const std::u16string& formName,
                           const GURL& pageURL, const GURL& frameOrigin,
                           scoped_refptr<FieldDataManager> fieldDataManager,
                           const std::string& frame_id, NSString* formJSON) {
    std::vector<FormData> formData;
    bool success = autofill::ExtractFormsData(
        formJSON, filtered, formName, pageURL, frameOrigin, *fieldDataManager,
        frame_id, &formData);
    std::move(completion).Run(success, formData);
  };
  AutofillJavaScriptFeature::GetInstance()->FetchForms(
      frame, base::BindOnce(callback, std::move(completionHandler), filtered,
                            formName, pageURL, frameOrigin, fieldDataManager,
                            frame->GetFrameId()));
}

- (void)onSuggestionsReady:(NSArray<FormSuggestion*>*)suggestions
        suggestionDelegate:
            (const base::WeakPtr<autofill::AutofillSuggestionDelegate>&)
                delegate {
  _suggestionDelegate = delegate;
  _mostRecentSuggestions = suggestions;
  if (SuggestionsAvailableCompletion completion =
          std::exchange(_suggestionsAvailableCompletion, nil)) {
    completion([_mostRecentSuggestions count] > 0);
  }
}

// Sends a request to BrowserAutofillManager to retrieve suggestions for the
// specified form and field.
- (void)queryAutofillForForm:(const FormData&)form
             fieldIdentifier:(FieldRendererId)fieldIdentifier
                        type:(NSString*)type
                  typedValue:(NSString*)typedValue
                       frame:(base::WeakPtr<web::WebFrame>)frame
                    webState:(base::WeakPtr<web::WebState>)webState
           completionHandler:(SuggestionsAvailableCompletion)completion {
  if (!frame || !webState) {
    completion(NO);
    return;
  }

  // Save the completion and go look for suggestions.
  _suggestionsAvailableCompletion = [completion copy];
  _typedValue = typedValue;

  // Query the BrowserAutofillManager for suggestions. Results will arrive in
  // -showAutofillPopup:suggestionDelegate:.
  if (!ContainsFocusableField(form, fieldIdentifier)) {
    return;
  }
  _lastQueriedFieldID = {form.host_frame(), fieldIdentifier};
  auto* driver = autofill::AutofillDriverIOS::FromWebStateAndWebFrame(
      _webState, frame.get());
  DLOG_IF(WARNING, !driver) << "No AutofillDriverIOS found for WebFrame";
  if (!driver) {
    return;
  }
  driver->AskForValuesToFill(form, _lastQueriedFieldID);
}

- (void)processPage:(web::WebState*)webState {
  web::WebFramesManager* frames_manager =
      AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(webState);
  if (!frames_manager->GetMainWebFrame()) {
    return;
  }
  [self processFrame:frames_manager->GetMainWebFrame() inWebState:webState];
  for (auto* frame : frames_manager->GetAllWebFrames()) {
    if (frame->IsMainFrame()) {
      continue;
    }
    [self processFrame:frame inWebState:webState];
  }
}

- (void)processFrame:(web::WebFrame*)frame inWebState:(web::WebState*)webState {
  if (!frame) {
    return;
  }

  autofill::AutofillDriverIOS* driver =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(webState, frame);
  DLOG_IF(WARNING, !driver) << "No AutofillDriverIOS found for WebFrame";
  // This process is only done once.
  if (!driver || driver->is_processed()) {
    return;
  }
  driver->set_processed(true);

  AutofillFormFeaturesJavaScriptFeature::GetInstance()
      ->SetAutofillAcrossIframes(
          frame, base::FeatureList::IsEnabled(
                     autofill::features::kAutofillAcrossIframesIos));

  AutofillFormFeaturesJavaScriptFeature::GetInstance()
      ->SetAutofillIsolatedContentWorld(
          frame,
          base::FeatureList::IsEnabled(kAutofillIsolatedWorldForJavascriptIos));

  if (frame->IsMainFrame()) {
    _suggestionDelegate.reset();
    _suggestionsAvailableCompletion = nil;
    _suggestionHandledCompletion = nil;
    _mostRecentSuggestions = nil;
    _typedValue = nil;
  }

  FormHandlersJavaScriptFeature* formHandlerFeature =
      FormHandlersJavaScriptFeature::GetInstance();

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

// Records if the renderer was able to fill the Autofill-provided values in a
// form or formless fields.
- (void)recordFormFillingSuccessMetrics:(BOOL)success {
  base::UmaHistogramBoolean(/*name=*/"Autofill.FormFillSuccessIOS",
                            /*sample=*/success);

  ukm::SourceId source_id = ukm::GetSourceIdForWebStateDocument(_webState);
  ukm::builders::Autofill_FormFillSuccessIOS(source_id)
      .SetFormFillSuccess(success)
      .Record(ukm::UkmRecorder::Get());
}

@end
