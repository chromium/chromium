// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_external_delegate.h"

#include <stddef.h>

#include <functional>
#include <iterator>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_compose_delegate.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/autofill_prediction_improvements_delegate.h"
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/field_filling_address_util.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/metrics/autofill_in_devtools_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/granular_filling_metrics.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/signatures.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"

namespace autofill {

namespace {

const AutofillProfile* GetTestAddressByGUID(
    base::span<const AutofillProfile> test_addresses,
    const std::string& guid) {
  if (test_addresses.empty()) {
    return nullptr;
  }
  auto it = base::ranges::find(test_addresses, guid, &AutofillProfile::guid);
  return it == test_addresses.end() ? nullptr : &(*it);
}

// The `AutofillTriggerSource` indicates what caused an Autofill fill or preview
// to happen. This can happen by selecting a suggestion, but also through a
// dynamic change (refills) or through a surface that doesn't use suggestions,
// like TTF. This function is concerned with the first case: A suggestion that
// was generated through the `suggestion_trigger_source` got selected. This
// function returns the appropriate `AutofillTriggerSource`.
// Note that an `AutofillSuggestionTriggerSource` is different from a
// `AutofillTriggerSource`. The former describes what caused the suggestion
// itself to appear. For example, depending on the completeness of the form,
// clicking into a field (the suggestion trigger source) can cause
// the keyboard accessory or TTF/fast checkout to appear (the trigger source).
AutofillTriggerSource TriggerSourceFromSuggestionTriggerSource(
    AutofillSuggestionTriggerSource suggestion_trigger_source) {
  switch (suggestion_trigger_source) {
    case AutofillSuggestionTriggerSource::kUnspecified:
    case AutofillSuggestionTriggerSource::kFormControlElementClicked:
    case AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick:
    case AutofillSuggestionTriggerSource::kContentEditableClicked:
    case AutofillSuggestionTriggerSource::kTextFieldDidChange:
    case AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown:
    case AutofillSuggestionTriggerSource::kOpenTextDataListChooser:
    case AutofillSuggestionTriggerSource::kShowCardsFromAccount:
    case AutofillSuggestionTriggerSource::kPasswordManager:
    case AutofillSuggestionTriggerSource::kiOS:
    case AutofillSuggestionTriggerSource::
        kShowPromptAfterDialogClosedNonManualFallback:
    case AutofillSuggestionTriggerSource::kComposeDialogLostFocus:
    case AutofillSuggestionTriggerSource::kComposeDelayedProactiveNudge:
    case AutofillSuggestionTriggerSource::kPasswordManagerProcessedFocusedField:
    case AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess:
      // On Android, no popup exists. Instead, the keyboard accessory is used.
#if BUILDFLAG(IS_ANDROID)
      return AutofillTriggerSource::kKeyboardAccessory;
#else
      return AutofillTriggerSource::kPopup;
#endif  // BUILDFLAG(IS_ANDROID)
    case AutofillSuggestionTriggerSource::kManualFallbackAddress:
    case AutofillSuggestionTriggerSource::kManualFallbackPayments:
    case AutofillSuggestionTriggerSource::kManualFallbackPasswords:
    case AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses:
      // Manual fallbacks are both a suggestion trigger source (e.g. through the
      // context menu) and a trigger source (by selecting a suggestion generated
      // through the context menu).
      return AutofillTriggerSource::kManualFallback;
    case AutofillSuggestionTriggerSource::kPredictionImprovements:
      return AutofillTriggerSource::kPredictionImprovements;
  }
  NOTREACHED();
}

// Returns a pointer to the first Suggestion whose GUID matches that of a
// AutofillClient::GetTestAddresses() profile.
const Suggestion* FindTestSuggestion(AutofillClient& client,
                                     base::span<const Suggestion> suggestions,
                                     int index) {
  auto is_test_suggestion = [&client](const Suggestion& suggestion) {
    auto* backend_id = absl::get_if<Suggestion::BackendId>(&suggestion.payload);
    auto* guid =
        backend_id ? absl::get_if<Suggestion::Guid>(backend_id) : nullptr;
    base::span<const AutofillProfile> test_addresses =
        client.GetTestAddresses();

    return guid && base::Contains(test_addresses, guid->value(),
                                  &AutofillProfile::guid);
  };
  for (const Suggestion& suggestion : suggestions) {
    if (is_test_suggestion(suggestion) && index-- == 0) {
      return &suggestion;
    }
  }
  return nullptr;
}

// Removes the warning suggestions if `suggestions` also contains suggestions
// that are not a warning.
void PossiblyRemoveAutofillWarnings(std::vector<Suggestion>& suggestions) {
  auto is_warning = [](const Suggestion& suggestion) {
    const SuggestionType type = suggestion.type;
    return type == SuggestionType::kInsecureContextPaymentDisabledMessage ||
           type == SuggestionType::kMixedFormMessage;
  };
  if (std::ranges::find_if(suggestions, std::not_fn(is_warning)) ==
      suggestions.end()) {
    return;
  }

  std::erase_if(suggestions, is_warning);
}

}  // namespace

int AutofillExternalDelegate::shortcut_test_suggestion_index_ = -1;

AutofillExternalDelegate::AutofillExternalDelegate(
    BrowserAutofillManager* manager)
    : manager_(CHECK_DEREF(manager)) {}

AutofillExternalDelegate::~AutofillExternalDelegate() = default;

// static
bool AutofillExternalDelegate::IsAutofillAndFirstLayerSuggestionId(
    SuggestionType item_id) {
  switch (item_id) {
    case SuggestionType::kAddressEntry:
    case SuggestionType::kFillFullAddress:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kCreditCardFieldByFieldFilling:
    case SuggestionType::kFillFullName:
    case SuggestionType::kFillFullPhoneNumber:
    case SuggestionType::kFillFullEmail:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kDevtoolsTestAddresses:
      // Virtual cards can appear on their own when filling the CVC for a card
      // that a merchant has saved. This indicates there could be Autofill
      // suggestions related to standalone CVC fields.
    case SuggestionType::kVirtualCreditCardEntry:
      return true;
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kAutocompleteEntry:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kCreateNewPlusAddress:
    case SuggestionType::kCreateNewPlusAddressInline:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kDeleteAddressProfile:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kEditAddressProfile:
    case SuggestionType::kFillEverythingFromAddressProfile:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kPasswordAccountStorageEmpty:
    case SuggestionType::kPasswordAccountStorageOptIn:
    case SuggestionType::kPasswordAccountStorageOptInAndGenerate:
    case SuggestionType::kPasswordAccountStorageReSignin:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kPlusAddressError:
    case SuggestionType::kPredictionImprovementsFeedback:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kShowAccountCards:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kRetrievePredictionImprovements:
    case SuggestionType::kPredictionImprovementsLoadingState:
    case SuggestionType::kFillPredictionImprovements:
    case SuggestionType::kPredictionImprovementsError:
    case SuggestionType::kEditPredictionImprovementsInformation:
      return false;
  }
}

void AutofillExternalDelegate::OnQuery(
    const FormData& form,
    const FormFieldData& field,
    const gfx::Rect& caret_bounds,
    AutofillSuggestionTriggerSource trigger_source) {
  query_form_ = form;
  query_field_ = field;
  caret_bounds_ = caret_bounds;
  trigger_source_ = trigger_source;
}

const AutofillField* AutofillExternalDelegate::GetQueriedAutofillField() const {
  return manager_->GetAutofillField(query_form_, query_field_);
}

void AutofillExternalDelegate::OnSuggestionsReturned(
    FieldGlobalId field_id,
    const std::vector<Suggestion>& input_suggestions,
    std::optional<autofill_metrics::SuggestionRankingContext>
        suggestion_ranking_context) {
  // These are guards against outdated suggestion results.
  if (field_id != query_field_.global_id()) {
    return;
  }
#if BUILDFLAG(IS_IOS)
  if (!manager_->client().IsLastQueriedField(field_id)) {
    return;
  }
#endif
  AttemptToDisplayAutofillSuggestions(
      input_suggestions, std::move(suggestion_ranking_context), trigger_source_,
      /*is_update=*/false);
}

void AutofillExternalDelegate::AttemptToDisplayAutofillSuggestions(
    std::vector<Suggestion> suggestions,
    std::optional<autofill_metrics::SuggestionRankingContext>
        suggestion_ranking_context,
    AutofillSuggestionTriggerSource trigger_source,
    bool is_update) {
  PossiblyRemoveAutofillWarnings(suggestions);
  // If anything else is added to modify the values after inserting the data
  // list, AutofillPopupControllerImpl::UpdateDataListValues will need to be
  // updated to match.
  InsertDataListValues(suggestions);

  // TODO(crbug.com/362630793): Try to eliminate this state. The controller
  // should be the one that knows about what suggestions were shown and passes
  // it on, not AED.
  trigger_source_ = trigger_source;

  suggestion_ranking_context_ = std::move(suggestion_ranking_context);
  shown_suggestion_types_.clear();
  for (const Suggestion& suggestion : suggestions) {
    shown_suggestion_types_.push_back(suggestion.type);
  }

  if (suggestions.empty()) {
    OnAutofillAvailabilityEvent(
        mojom::AutofillSuggestionAvailability::kNoSuggestions);
    // No suggestions, any popup currently showing is obsolete.
    manager_->client().HideAutofillSuggestions(
        SuggestionHidingReason::kNoSuggestions);
    return;
  }

  if (!query_field_.is_focusable() || !manager_->driver().CanShowAutofillUi()) {
    return;
  }

  if (shortcut_test_suggestion_index_ >= 0) {
    const Suggestion* test_suggestion = FindTestSuggestion(
        manager_->client(), suggestions, shortcut_test_suggestion_index_);
    CHECK(test_suggestion) << "Only test suggestions can shortcut the UI";
    DidAcceptSuggestion(*test_suggestion, {});
    return;
  }

  // Send to display.
  if (is_update) {
    manager_->client().UpdateAutofillSuggestions(
        suggestions, GetMainFillingProduct(), trigger_source_);
    return;
  }

  AutofillComposeDelegate* delegate = manager_->client().GetComposeDelegate();
  const bool show_proactive_nudge_at_caret =
      shown_suggestion_types_.size() == 1 &&
      shown_suggestion_types_[0] == SuggestionType::kComposeProactiveNudge &&
      (delegate && delegate->ShouldAnchorNudgeOnCaret());
  const bool are_caret_bounds_valid =
      caret_bounds_ != gfx::Rect() &&
      query_field_.bounds().Contains(gfx::RectF(caret_bounds_));
  const bool should_use_caret_bounds =
      show_proactive_nudge_at_caret && are_caret_bounds_valid;

  const PopupAnchorType default_anchor_type =
#if BUILDFLAG(IS_ANDROID)
      PopupAnchorType::kKeyboardAccessory;
#else
      PopupAnchorType::kField;
#endif
  AutofillClient::PopupOpenArgs open_args(
      should_use_caret_bounds ? gfx::RectF(caret_bounds_)
                              : query_field_.bounds(),
      query_field_.text_direction(), suggestions, trigger_source_,
      query_field_.form_control_ax_id(),
      should_use_caret_bounds ? PopupAnchorType::kCaret : default_anchor_type);
  manager_->client().ShowAutofillSuggestions(open_args, GetWeakPtr());
}

base::RepeatingCallback<void(std::vector<Suggestion>,
                             AutofillSuggestionTriggerSource)>
AutofillExternalDelegate::CreateUpdateSuggestionsCallback() {
  using SessionId = AutofillClient::SuggestionUiSessionId;
  const std::optional<SessionId> session_id =
      manager_->client().GetSessionIdForCurrentAutofillSuggestions();
  if (!session_id) {
    return base::DoNothing();
  }
  return base::BindRepeating(
      [](base::WeakPtr<AutofillExternalDelegate> self, SessionId session_id,
         std::vector<Suggestion> suggestions,
         AutofillSuggestionTriggerSource trigger_source) {
        if (!self) {
          return;
        }
        if (self->manager_->client()
                .GetSessionIdForCurrentAutofillSuggestions()
                .value_or(SessionId()) != session_id) {
          return;
        }
        self->AttemptToDisplayAutofillSuggestions(
            std::move(suggestions),
            /*suggestion_ranking_context=*/std::nullopt, trigger_source,
            /*is_update=*/true);
      },
      GetWeakPtr(), *session_id);
}

base::OnceCallback<void(SuggestionHidingReason)>
AutofillExternalDelegate::CreateHideSuggestionsCallback() {
  using SessionId = AutofillClient::SuggestionUiSessionId;
  const std::optional<SessionId> session_id =
      manager_->client().GetSessionIdForCurrentAutofillSuggestions();
  if (!session_id) {
    return base::DoNothing();
  }
  return base::BindOnce(
      [](base::WeakPtr<AutofillExternalDelegate> self, SessionId session_id,
         SuggestionHidingReason hiding_reason) {
        if (!self) {
          return;
        }
        if (self->manager_->client()
                .GetSessionIdForCurrentAutofillSuggestions()
                .value_or(SessionId()) != session_id) {
          return;
        }
        self->manager_->client().HideAutofillSuggestions(hiding_reason);
      },
      GetWeakPtr(), *session_id);
}

base::RepeatingCallback<void(const std::u16string&)>
AutofillExternalDelegate::CreateSingleFieldFillCallback(
    SuggestionType suggestion_type,
    std::optional<FieldType> field_type_used) {
  return base::BindRepeating(
      [](base::WeakPtr<AutofillExternalDelegate> self, const FormData& form,
         const FormFieldData& field, SuggestionType suggestion_type,
         std::optional<FieldType> field_type_used,
         const std::u16string& value) {
        if (!self) {
          return;
        }
        self->manager_->FillOrPreviewField(mojom::ActionPersistence::kFill,
                                           mojom::FieldActionType::kReplaceAll,
                                           form, field, value, suggestion_type,
                                           field_type_used);
      },
      GetWeakPtr(), query_form_, query_field_, suggestion_type,
      field_type_used);
}

SuggestionType
AutofillExternalDelegate::GetLastAcceptedSuggestionToFillForSection(
    const Section& section) const {
  if (auto it = last_accepted_address_suggestion_for_address_form_section_.find(
          section);
      it != last_accepted_address_suggestion_for_address_form_section_.end()) {
    return it->second;
  }
  // In case no suggestions were accepted for this section, default to full form
  // filling suggestions.
  return SuggestionType::kAddressEntry;
}

bool AutofillExternalDelegate::HasActiveScreenReader() const {
#if BUILDFLAG(IS_IOS)
  // ui::AXPlatform is not supported on iOS. The rendering engine handles
  // a11y internally.
  return false;
#else
  // Note: This always returns false if ChromeVox is in use because the
  // process-wide AXMode is not updated in that case; except for Lacros, where
  // kScreenReader mirrors the spoken feedback preference.
  return ui::AXPlatform::GetInstance().GetMode().has_mode(
      ui::AXMode::kScreenReader);
#endif
}

void AutofillExternalDelegate::OnAutofillAvailabilityEvent(
    mojom::AutofillSuggestionAvailability suggestion_availability) {
  // Availability of suggestions should be communicated to Blink because
  // accessibility objects live in both the renderer and browser processes.
  manager_->driver().RendererShouldSetSuggestionAvailability(
      query_field_.global_id(), suggestion_availability);
}

void AutofillExternalDelegate::SetCurrentDataListValues(
    std::vector<SelectOption> datalist) {
  datalist_ = std::move(datalist);
  manager_->client().UpdateAutofillDataListValues(datalist_);
}

absl::variant<AutofillDriver*, password_manager::PasswordManagerDriver*>
AutofillExternalDelegate::GetDriver() {
  return &manager_->driver();
}

void AutofillExternalDelegate::OnSuggestionsShown(
    base::span<const Suggestion> suggestions) {
  // Popups are expected to be Autofill or Autocomplete.
  DCHECK(suggestions.empty() ||
         GetFillingProductFromSuggestionType(suggestions[0].type) !=
             FillingProduct::kPassword);

  const DenseSet<SuggestionType> shown_suggestion_types(suggestions,
                                                        &Suggestion::type);
  const bool has_autofill_suggestions = std::ranges::any_of(
      shown_suggestion_types, IsAutofillAndFirstLayerSuggestionId);

  if (shown_suggestion_types.contains(
          SuggestionType::kCreateNewPlusAddressInline)) {
    if (auto* plus_address_delegate =
            manager_->client().GetPlusAddressDelegate()) {
      plus_address_delegate->OnShowedInlineSuggestion(
          manager_->client().GetLastCommittedPrimaryMainFrameOrigin(),
          suggestions, CreateUpdateSuggestionsCallback());
    }
  }

  if (shown_suggestion_types.contains(
          SuggestionType::kPredictionImprovementsLoadingState)) {
    if (auto* prediction_improvements_delegate =
            manager_->client().GetAutofillPredictionImprovementsDelegate()) {
      prediction_improvements_delegate->OnLoadingSuggestionShown(
          query_form_, query_field_, CreateUpdateSuggestionsCallback());
    }
  }

  // If the popup was manually triggered on an unclassified field, the chances
  // are high that it has no regular suggestions, as it is the main usecase for
  // the manual fallback functionality. It is considered an acceptable
  // approximation, but it obviously doesn't cover many other cases, such as
  // manual triggering payment suggestions on an address field, and should
  // be reconsidered if some more critical cases are found.
  const bool likely_has_no_regular_autofilling_options =
      TriggerSourceFromSuggestionTriggerSource(trigger_source_) ==
          AutofillTriggerSource::kManualFallback &&
      (!GetQueriedAutofillField() ||
       GetQueriedAutofillField()->Type().IsUnknown());

  if (likely_has_no_regular_autofilling_options) {
    OnAutofillAvailabilityEvent(
        mojom::AutofillSuggestionAvailability::kNoSuggestions);
  } else if (has_autofill_suggestions) {
    OnAutofillAvailabilityEvent(
        mojom::AutofillSuggestionAvailability::kAutofillAvailable);
    if (shown_suggestion_types.contains(
            SuggestionType::kDevtoolsTestAddresses)) {
      autofill_metrics::OnDevtoolsTestAddressesShown();
    }
  } else {
    // We send autocomplete availability event even though there might be no
    // autocomplete suggestions shown.
    // TODO(crbug.com/315748930): Provide AX event only for autocomplete
    // entries.
    OnAutofillAvailabilityEvent(
        mojom::AutofillSuggestionAvailability::kAutocompleteAvailable);
    if (shown_suggestion_types.contains(SuggestionType::kAutocompleteEntry)) {
      AutofillMetrics::OnAutocompleteSuggestionsShown();
    }
  }

  manager_->DidShowSuggestions(shown_suggestion_types, query_form_,
                               query_field_);

  if (shown_suggestion_types.contains(SuggestionType::kShowAccountCards)) {
    autofill_metrics::LogAutofillShowCardsFromGoogleAccountButtonEventMetric(
        autofill_metrics::ShowCardsFromGoogleAccountButtonEvent::
            kButtonAppeared);
    if (!std::exchange(show_cards_from_account_suggestion_was_shown_, true)) {
      autofill_metrics::LogAutofillShowCardsFromGoogleAccountButtonEventMetric(
          autofill_metrics::ShowCardsFromGoogleAccountButtonEvent::
              kButtonAppearedOnce);
    }
  }

  if (shown_suggestion_types.contains(SuggestionType::kScanCreditCard)) {
    AutofillMetrics::LogScanCreditCardPromptMetric(
        AutofillMetrics::SCAN_CARD_ITEM_SHOWN);
  }
}

void AutofillExternalDelegate::OnSuggestionsHidden() {
  manager_->OnSuggestionsHidden();
}

void AutofillExternalDelegate::DidSelectSuggestion(
    const Suggestion& suggestion) {
  ClearPreviewedForm();

  const Suggestion::BackendId backend_id =
      suggestion.GetPayload<Suggestion::BackendId>();

  switch (suggestion.type) {
    case SuggestionType::kUndoOrClear:
#if !BUILDFLAG(IS_IOS)
      manager_->UndoAutofill(mojom::ActionPersistence::kPreview, query_form_,
                             query_field_);
#endif
      break;
    case SuggestionType::kAddressEntry:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kFillEverythingFromAddressProfile:
    case SuggestionType::kDevtoolsTestAddressEntry:
      FillAutofillFormData(
          suggestion.type, backend_id, /*metadata=*/std::nullopt,
          /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      break;
    case SuggestionType::kFillFullAddress:
      FillAutofillFormData(
          suggestion.type, backend_id,
          /*metadata=*/std::nullopt, /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill = GetAddressFieldsForGroupFilling()});
      break;
    case SuggestionType::kFillFullName:
      FillAutofillFormData(
          suggestion.type, backend_id,
          /*metadata=*/std::nullopt, /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill = GetFieldTypesOfGroup(FieldTypeGroup::kName)});
      break;
    case SuggestionType::kFillFullPhoneNumber:
      FillAutofillFormData(
          suggestion.type, backend_id,
          /*metadata=*/std::nullopt, /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill =
               GetFieldTypesOfGroup(FieldTypeGroup::kPhone)});
      break;
    case SuggestionType::kFillFullEmail:
      FillAutofillFormData(
          suggestion.type, backend_id,
          /*metadata=*/std::nullopt, /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill =
               GetFieldTypesOfGroup(FieldTypeGroup::kEmail)});
      break;
    case SuggestionType::kAutocompleteEntry:
      manager_->FillOrPreviewField(mojom::ActionPersistence::kPreview,
                                   mojom::FieldActionType::kReplaceAll,
                                   query_form_, query_field_,
                                   suggestion.main_text.value, suggestion.type,
                                   /*field_type_used=*/std::nullopt);
      break;
    case SuggestionType::kIbanEntry:
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kPreview,
          mojom::FieldActionType::kReplaceAll, query_form_, query_field_,
          suggestion.main_text.value, suggestion.type, IBAN_VALUE);
      break;
    case SuggestionType::kMerchantPromoCodeEntry:
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kPreview,
          mojom::FieldActionType::kReplaceAll, query_form_, query_field_,
          suggestion.main_text.value, suggestion.type, MERCHANT_PROMO_CODE);
      break;
    case SuggestionType::kFillExistingPlusAddress:
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kPreview,
          mojom::FieldActionType::kReplaceAll, query_form_, query_field_,
          suggestion.main_text.value, suggestion.type, EMAIL_ADDRESS);
      break;
    case SuggestionType::kCreateNewPlusAddressInline:
      if (std::optional<std::u16string> plus_address =
              suggestion.GetPayload<Suggestion::PlusAddressPayload>().address) {
        manager_->FillOrPreviewField(mojom::ActionPersistence::kPreview,
                                     mojom::FieldActionType::kReplaceAll,
                                     query_form_, query_field_, *plus_address,
                                     suggestion.type, EMAIL_ADDRESS);
      }
      break;
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kCreditCardFieldByFieldFilling:
      PreviewFieldByFieldFillingSuggestion(suggestion);
      break;
    case SuggestionType::kVirtualCreditCardEntry:
      // If triggered on a non payments form, don't preview the value.
      if (IsPaymentsManualFallbackOnNonPaymentsField()) {
        break;
      }
      FillAutofillFormData(
          suggestion.type, backend_id, /*metadata=*/std::nullopt,
          /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      break;
    case SuggestionType::kFillPredictionImprovements:
      // TODO(crbug.com/361414075): Implement previewing prediction
      // improvements.
      break;
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kCreateNewPlusAddress:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kDeleteAddressProfile:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kEditAddressProfile:
    case SuggestionType::kPredictionImprovementsError:
    case SuggestionType::kEditPredictionImprovementsInformation:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kPlusAddressError:
    case SuggestionType::kPredictionImprovementsLoadingState:
    case SuggestionType::kRetrievePredictionImprovements:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kShowAccountCards:
      break;
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kPasswordAccountStorageEmpty:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kPasswordAccountStorageOptIn:
    case SuggestionType::kPasswordAccountStorageReSignin:
    case SuggestionType::kPasswordAccountStorageOptInAndGenerate:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kPredictionImprovementsFeedback:
    case SuggestionType::kViewPasswordDetails:
      NOTREACHED();  // Should be handled elsewhere.
  }
}

void AutofillExternalDelegate::DidAcceptSuggestion(
    const Suggestion& suggestion,
    const SuggestionMetadata& metadata) {
  CHECK(suggestion.is_acceptable);
  base::UmaHistogramEnumeration("Autofill.Suggestions.AcceptedType",
                                suggestion.type);

  switch (suggestion.type) {
    case SuggestionType::kAddressEntry:
    case SuggestionType::kFillFullAddress:
    case SuggestionType::kFillFullName:
    case SuggestionType::kFillEverythingFromAddressProfile:
    case SuggestionType::kFillFullPhoneNumber:
    case SuggestionType::kFillFullEmail:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kEditAddressProfile:
    case SuggestionType::kDeleteAddressProfile:
    case SuggestionType::kDevtoolsTestAddressEntry:
      DidAcceptAddressSuggestion(suggestion, metadata);
      break;
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kCreditCardFieldByFieldFilling:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kScanCreditCard:
      DidAcceptPaymentsSuggestion(suggestion, metadata);
      break;
    case SuggestionType::kShowAccountCards:
      DidAcceptPaymentsSuggestion(suggestion, metadata);
      manager_->RefetchCardsAndUpdatePopup(query_form_, query_field_);
      return;
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManagePlusAddress: {
      manager_->client().ShowAutofillSettings(suggestion.type);
      break;
    }
    case SuggestionType::kUndoOrClear:
#if !BUILDFLAG(IS_IOS)
      manager_->UndoAutofill(mojom::ActionPersistence::kFill, query_form_,
                             query_field_);
#endif
      break;
    case SuggestionType::kDatalistEntry:
      manager_->driver().RendererShouldAcceptDataListSuggestion(
          query_field_.global_id(), suggestion.main_text.value);
      break;
    case SuggestionType::kAutocompleteEntry:
      AutofillMetrics::LogAutocompleteEvent(
          AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_SELECTED);
      autofill_metrics::LogSuggestionAcceptedIndex(
          metadata.row, FillingProduct::kAutocomplete,
          manager_->client().IsOffTheRecord());
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          query_form_, query_field_, suggestion.main_text.value,
          suggestion.type, /*field_type_used=*/std::nullopt);
      manager_->OnSingleFieldSuggestionSelected(suggestion, query_form_,
                                                query_field_);
      break;
    case SuggestionType::kFillExistingPlusAddress:
      if (AutofillPlusAddressDelegate* plus_address_delegate =
              manager_->client().GetPlusAddressDelegate()) {
        plus_address_delegate->RecordAutofillSuggestionEvent(
            AutofillPlusAddressDelegate::SuggestionEvent::
                kExistingPlusAddressChosen);
      }
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          query_form_, query_field_, suggestion.main_text.value,
          SuggestionType::kFillExistingPlusAddress, EMAIL_ADDRESS);
      break;
    case SuggestionType::kCreateNewPlusAddress: {
      if (AutofillPlusAddressDelegate* plus_address_delegate =
              manager_->client().GetPlusAddressDelegate()) {
        plus_address_delegate->RecordAutofillSuggestionEvent(
            AutofillPlusAddressDelegate::SuggestionEvent::
                kCreateNewPlusAddressChosen);
      }
      manager_->client().OfferPlusAddressCreation(
          manager_->client().GetLastCommittedPrimaryMainFrameOrigin(),
          CreatePlusAddressCallback(SuggestionType::kCreateNewPlusAddress));
      break;
    }
    case SuggestionType::kCreateNewPlusAddressInline:
      DidAcceptCreateNewPlusAddressInlineSuggestion(suggestion);
      // The delegate handles hiding the popup.
      return;
    case SuggestionType::kPlusAddressError:
      break;
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeSavedStateNotification:
      if (AutofillComposeDelegate* delegate =
              manager_->client().GetComposeDelegate()) {
        delegate->OpenCompose(
            manager_->driver(), query_field_.renderer_form_id(),
            query_field_.global_id(),
            autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup);
      }
      break;
    case SuggestionType::kComposeDisable:
      if (AutofillComposeDelegate* delegate =
              manager_->client().GetComposeDelegate()) {
        delegate->DisableCompose();
      }
      break;
    case SuggestionType::kComposeGoToSettings:
      if (AutofillComposeDelegate* delegate =
              manager_->client().GetComposeDelegate()) {
        delegate->GoToSettings();
      }
      break;
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
      if (AutofillComposeDelegate* delegate =
              manager_->client().GetComposeDelegate()) {
        delegate->NeverShowComposeForOrigin(
            manager_->client().GetLastCommittedPrimaryMainFrameOrigin());
      }
      break;
    case SuggestionType::kRetrievePredictionImprovements:
      if (AutofillPredictionImprovementsDelegate* delegate =
              manager_->client().GetAutofillPredictionImprovementsDelegate()) {
        delegate->OnClickedTriggerSuggestion(query_form_, query_field_,
                                             CreateUpdateSuggestionsCallback());
      }
      return;
    case SuggestionType::kFillPredictionImprovements:
      FillPredictionImprovements(suggestion);
      break;
    case SuggestionType::kEditPredictionImprovementsInformation:
      if (AutofillPredictionImprovementsDelegate* delegate =
              manager_->client().GetAutofillPredictionImprovementsDelegate()) {
        delegate->GoToSettings();
      }
      break;
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kMixedFormMessage:
      // If the selected element is a warning we don't want to do anything.
      break;
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kPasswordAccountStorageEmpty:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kPasswordAccountStorageOptIn:
    case SuggestionType::kPasswordAccountStorageReSignin:
    case SuggestionType::kPasswordAccountStorageOptInAndGenerate:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kPredictionImprovementsFeedback:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kPredictionImprovementsLoadingState:
    case SuggestionType::kPredictionImprovementsError:
      NOTREACHED();  // Should be handled elsewhere.
  }
  // Note that some suggestion types return early.
  manager_->client().HideAutofillSuggestions(
      SuggestionHidingReason::kAcceptSuggestion);
}

void AutofillExternalDelegate::DidPerformButtonActionForSuggestion(
    const Suggestion& suggestion,
    const SuggestionButtonAction& button_action) {
  switch (suggestion.type) {
    case SuggestionType::kComposeResumeNudge:
      NOTIMPLEMENTED();
      return;
    case SuggestionType::kCreateNewPlusAddressInline:
    case SuggestionType::kPlusAddressError:
      if (AutofillPlusAddressDelegate* plus_address_delegate =
              manager_->client().GetPlusAddressDelegate()) {
        ClearPreviewedForm();
        base::span<const Suggestion> suggestions =
            manager_->client().GetAutofillSuggestions();
        // TODO(crbug.com/362445807): Change the signature of
        // DidPerformButtonActionForSuggestion to pass all suggestions and an
        // index of the currently focused one.
        auto it = std::ranges::find(suggestions, suggestion);
        CHECK(it != suggestions.end());
        plus_address_delegate->OnClickedRefreshInlineSuggestion(
            manager_->client().GetLastCommittedPrimaryMainFrameOrigin(),
            manager_->client().GetAutofillSuggestions(),
            /*current_suggestion_index=*/it - suggestions.begin(),
            CreateUpdateSuggestionsCallback());
      }
      return;
    case SuggestionType::kPredictionImprovementsFeedback: {
      AutofillPredictionImprovementsDelegate* delegate =
          manager_->client().GetAutofillPredictionImprovementsDelegate();
      if (!delegate) {
        break;
      }
      CHECK(absl::holds_alternative<PredictionImprovementsButtonActions>(
          button_action));
      PredictionImprovementsButtonActions action =
          absl::get<PredictionImprovementsButtonActions>(button_action);
      switch (action) {
        case PredictionImprovementsButtonActions::kThumbsUpClicked:
          delegate->UserFeedbackReceived(
              AutofillPredictionImprovementsDelegate::UserFeedback::kThumbsUp);
          break;
        case PredictionImprovementsButtonActions::kThumbsDownClicked:
          delegate->UserFeedbackReceived(
              AutofillPredictionImprovementsDelegate::UserFeedback::
                  kThumbsDown);
          break;
        case PredictionImprovementsButtonActions::kLearnMoreClicked:
          delegate->UserClickedLearnMore();
          break;
      }
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

bool AutofillExternalDelegate::RemoveSuggestion(const Suggestion& suggestion) {
  switch (suggestion.type) {
    // These SuggestionTypes are various types which can appear in the first
    // level suggestion to fill an address or credit card field.
    case SuggestionType::kAddressEntry:
    case SuggestionType::kFillFullAddress:
    case SuggestionType::kFillFullName:
    case SuggestionType::kFillFullEmail:
    case SuggestionType::kFillFullPhoneNumber:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kCreditCardFieldByFieldFilling:
    case SuggestionType::kCreditCardEntry:
      return manager_->RemoveAutofillProfileOrCreditCard(
          suggestion.GetPayload<Suggestion::BackendId>());
    case SuggestionType::kAutocompleteEntry:
      manager_->RemoveCurrentSingleFieldSuggestion(
          query_field_.name(), suggestion.main_text.value, suggestion.type);
      return true;
    case SuggestionType::kFillEverythingFromAddressProfile:
    case SuggestionType::kEditAddressProfile:
    case SuggestionType::kDeleteAddressProfile:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kCreateNewPlusAddress:
    case SuggestionType::kCreateNewPlusAddressInline:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kPlusAddressError:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kShowAccountCards:
    case SuggestionType::kPasswordAccountStorageOptIn:
    case SuggestionType::kPasswordAccountStorageOptInAndGenerate:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kPasswordAccountStorageReSignin:
    case SuggestionType::kPasswordAccountStorageEmpty:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kPredictionImprovementsFeedback:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kRetrievePredictionImprovements:
    case SuggestionType::kPredictionImprovementsLoadingState:
    case SuggestionType::kFillPredictionImprovements:
    case SuggestionType::kPredictionImprovementsError:
    case SuggestionType::kEditPredictionImprovementsInformation:
      return false;
  }
}

void AutofillExternalDelegate::DidEndTextFieldEditing() {
  manager_->client().HideAutofillSuggestions(
      SuggestionHidingReason::kEndEditing);
}

void AutofillExternalDelegate::ClearPreviewedForm() {
  manager_->driver().RendererShouldClearPreviewedForm();
}

FillingProduct AutofillExternalDelegate::GetMainFillingProduct() const {
  bool has_plus_address_suggestion = false;
  for (SuggestionType type : shown_suggestion_types_) {
    if (FillingProduct product = GetFillingProductFromSuggestionType(type);
        product != FillingProduct::kNone) {
      // Plus address is considered to be the main filling product of the popup
      // only if it is the only fillable type in the suggestions list.
      if (product != FillingProduct::kPlusAddresses) {
        return product;
      }
      has_plus_address_suggestion = true;
    }
  }
  return has_plus_address_suggestion ? FillingProduct::kPlusAddresses
                                     : FillingProduct::kNone;
}

base::WeakPtr<AutofillExternalDelegate> AutofillExternalDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillExternalDelegate::ShowEditAddressProfileDialog(
    const std::string& guid) {
  const AutofillProfile* profile = manager_->client()
                                       .GetPersonalDataManager()
                                       ->address_data_manager()
                                       .GetProfileByGUID(guid);
  if (profile) {
    manager_->client().ShowEditAddressProfileDialog(
        *profile,
        base::BindOnce(&AutofillExternalDelegate::OnAddressEditorClosed,
                       GetWeakPtr()));
  }
}

void AutofillExternalDelegate::ShowDeleteAddressProfileDialog(
    const std::string& guid) {
  const AutofillProfile* profile = manager_->client()
                                       .GetPersonalDataManager()
                                       ->address_data_manager()
                                       .GetProfileByGUID(guid);
  if (profile) {
    manager_->client().ShowDeleteAddressProfileDialog(
        *profile,
        base::BindOnce(&AutofillExternalDelegate::OnDeleteDialogClosed,
                       GetWeakPtr(), guid));
  }
}

void AutofillExternalDelegate::OnAddressEditorClosed(
    AutofillClient::AddressPromptUserDecision decision,
    base::optional_ref<const AutofillProfile> edited_profile) {
  if (decision == AutofillClient::AddressPromptUserDecision::kEditAccepted) {
    autofill_metrics::LogEditAddressProfileDialogClosed(
        /*user_saved_changes=*/true);
    AddressDataManager& adm =
        manager_->client().GetPersonalDataManager()->address_data_manager();
    if (!adm_observation_.IsObserving()) {
      adm_observation_.Observe(&adm);
    }
    CHECK(edited_profile.has_value());
    adm.UpdateProfile(edited_profile.value());
    return;
  }
  autofill_metrics::LogEditAddressProfileDialogClosed(
      /*user_saved_changes=*/false);
  manager_->driver().RendererShouldTriggerSuggestions(query_field_.global_id(),
                                                      GetReopenTriggerSource());
}

void AutofillExternalDelegate::OnDeleteDialogClosed(const std::string& guid,
                                                    bool user_accepted_delete) {
  autofill_metrics::LogDeleteAddressProfileFromExtendedMenu(
      user_accepted_delete);
  if (user_accepted_delete) {
    AddressDataManager& adm =
        manager_->client().GetPersonalDataManager()->address_data_manager();
    if (!adm_observation_.IsObserving()) {
      adm_observation_.Observe(&adm);
    }
    adm.RemoveProfile(guid);
    return;
  }
  manager_->driver().RendererShouldTriggerSuggestions(query_field_.global_id(),
                                                      GetReopenTriggerSource());
}

void AutofillExternalDelegate::OnAddressDataChanged() {
  adm_observation_.Reset();
  manager_->driver().RendererShouldTriggerSuggestions(query_field_.global_id(),
                                                      GetReopenTriggerSource());
}

void AutofillExternalDelegate::OnCreditCardScanned(
    const AutofillTriggerSource trigger_source,
    const CreditCard& card) {
  manager_->FillOrPreviewCreditCardForm(
      mojom::ActionPersistence::kFill, query_form_, query_field_, card,
      std::u16string(), {.trigger_source = trigger_source});
}

void AutofillExternalDelegate::PreviewFieldByFieldFillingSuggestion(
    const Suggestion& suggestion) {
  CHECK(suggestion.type == SuggestionType::kAddressFieldByFieldFilling ||
        suggestion.type == SuggestionType::kCreditCardFieldByFieldFilling);
  CHECK(suggestion.field_by_field_filling_type_used);
  const auto guid = suggestion.GetBackendId<Suggestion::Guid>().value();
  if (const AutofillProfile* profile = manager_->client()
                                           .GetPersonalDataManager()
                                           ->address_data_manager()
                                           .GetProfileByGUID(guid)) {
    PreviewAddressFieldByFieldFillingSuggestion(*profile, suggestion);
  } else if (manager_->client()
                 .GetPersonalDataManager()
                 ->payments_data_manager()
                 .GetCreditCardByGUID(guid)) {
    PreviewCreditCardFieldByFieldFillingSuggestion(suggestion);
  }
}

void AutofillExternalDelegate::FillFieldByFieldFillingSuggestion(
    const Suggestion& suggestion,
    const SuggestionMetadata& metadata) {
  CHECK(suggestion.type == SuggestionType::kAddressFieldByFieldFilling ||
        suggestion.type == SuggestionType::kCreditCardFieldByFieldFilling);
  CHECK(suggestion.field_by_field_filling_type_used);
  const auto guid = suggestion.GetBackendId<Suggestion::Guid>().value();
  if (const AutofillProfile* profile = manager_->client()
                                           .GetPersonalDataManager()
                                           ->address_data_manager()
                                           .GetProfileByGUID(guid)) {
    FillAddressFieldByFieldFillingSuggestion(*profile, suggestion, metadata);
  } else if (const CreditCard* credit_card = manager_->client()
                                                 .GetPersonalDataManager()
                                                 ->payments_data_manager()
                                                 .GetCreditCardByGUID(guid)) {
    FillCreditCardFieldByFieldFillingSuggestion(*credit_card, suggestion);
  }
}

void AutofillExternalDelegate::PreviewAddressFieldByFieldFillingSuggestion(
    const AutofillProfile& profile,
    const Suggestion& suggestion) {
  const auto& [filling_value, filling_type] = GetFillingValueAndTypeForProfile(
      profile, manager_->app_locale(),
      AutofillType(*suggestion.field_by_field_filling_type_used), query_field_,
      manager_->client().GetAddressNormalizer());
  if (!filling_value.empty()) {
    manager_->FillOrPreviewField(
        mojom::ActionPersistence::kPreview, mojom::FieldActionType::kReplaceAll,
        query_form_, query_field_, filling_value, suggestion.type,
        suggestion.field_by_field_filling_type_used);
  }
}

void AutofillExternalDelegate::FillAddressFieldByFieldFillingSuggestion(
    const AutofillProfile& profile,
    const Suggestion& suggestion,
    const SuggestionMetadata& metadata) {
  const AutofillField* autofill_trigger_field = GetQueriedAutofillField();
  if (autofill_trigger_field && metadata.sub_popup_level > 0) {
    // We only update this when the user accepts a subpopup suggestion since the
    // filling granularity doesn't change by accepting a top-level popup
    // suggestion but stays the same.
    last_accepted_address_suggestion_for_address_form_section_
        [autofill_trigger_field->section()] =
            SuggestionType::kAddressFieldByFieldFilling;
  }
  const bool is_triggering_field_address =
      autofill_trigger_field &&
      IsAddressType(autofill_trigger_field->Type().GetStorableType());

  autofill_metrics::LogFillingMethodUsed(
      FillingMethod::kFieldByFieldFilling, FillingProduct::kAddress,
      /*triggering_field_type_matches_filling_product=*/
      is_triggering_field_address);

  // Only log the field-by-field filling type used if it was accepted from
  // a suggestion in a subpopup. The root popup can have field-by-field
  // suggestions after a field-by-field suggestion was accepted from a
  // subpopup, this is done to keep the user in a certain filling
  // granularity during their filling experience. However only the
  // subpopups field-by-field-filling types are statically built, based on
  // what we think is useful/handy (this will in the future vary per
  // country, see crbug.com/1502162), while field-by-field filling
  // suggestions in the root popup are dynamically built depending on the
  // triggering field type, which means that selecting them is the only
  // option users have in the first level. Therefore we only emit logs for
  // subpopup acceptance to measure the efficiency of the types we chose
  // and potentially remove/add new ones.
  if (metadata.sub_popup_level > 0) {
    autofill_metrics::LogFieldByFieldFillingFieldUsed(
        *suggestion.field_by_field_filling_type_used, FillingProduct::kAddress,
        /*triggering_field_type_matches_filling_product=*/
        is_triggering_field_address);
  }

  const auto& [filling_value, filling_type] = GetFillingValueAndTypeForProfile(
      profile, manager_->app_locale(),
      AutofillType(*suggestion.field_by_field_filling_type_used), query_field_,
      manager_->client().GetAddressNormalizer());
  if (!filling_value.empty()) {
    manager_->FillOrPreviewField(
        mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
        query_form_, query_field_, filling_value, suggestion.type,
        suggestion.field_by_field_filling_type_used);
    manager_->OnDidFillAddressFormFillingSuggestion(
        profile, query_form_, query_field_,
        TriggerSourceFromSuggestionTriggerSource(trigger_source_));
  }
}

void AutofillExternalDelegate::PreviewCreditCardFieldByFieldFillingSuggestion(
    const Suggestion& suggestion) {
  manager_->FillOrPreviewField(
      mojom::ActionPersistence::kPreview, mojom::FieldActionType::kReplaceAll,
      query_form_, query_field_, suggestion.main_text.value, suggestion.type,
      suggestion.field_by_field_filling_type_used);
}

void AutofillExternalDelegate::FillCreditCardFieldByFieldFillingSuggestion(
    const CreditCard& credit_card,
    const Suggestion& suggestion) {
  if (*suggestion.field_by_field_filling_type_used == CREDIT_CARD_NUMBER) {
    manager_->GetCreditCardAccessManager().FetchCreditCard(
        &credit_card,
        base::BindOnce(&AutofillExternalDelegate::OnCreditCardFetched,
                       GetWeakPtr()));
    return;
  }
  manager_->FillOrPreviewField(
      mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
      query_form_, query_field_, suggestion.main_text.value, suggestion.type,
      suggestion.field_by_field_filling_type_used);
}

void AutofillExternalDelegate::OnCreditCardFetched(
    CreditCardFetchResult result,
    const CreditCard* credit_card) {
  if (result != CreditCardFetchResult::kSuccess) {
    return;
  }
  // In the failure case, `credit_card` can be `nullptr`, but in the success
  // case it is non-null.
  CHECK(credit_card);

  manager_->OnCreditCardFetchedSuccessfully(*credit_card);
  manager_->FillOrPreviewField(
      mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
      query_form_, query_field_,
      credit_card->GetInfo(CREDIT_CARD_NUMBER, manager_->app_locale()),
      SuggestionType::kCreditCardFieldByFieldFilling, CREDIT_CARD_NUMBER);
}

void AutofillExternalDelegate::OnVirtualCreditCardFetched(
    CreditCardFetchResult result,
    const CreditCard* credit_card) {
  if (result != CreditCardFetchResult::kSuccess) {
    return;
  }
  // In the failure case, `credit_card` can be `nullptr`, but in the success
  // case it is non-null.
  CHECK(credit_card);
  manager_->OnCreditCardFetchedSuccessfully(*credit_card);
}

void AutofillExternalDelegate::FillAutofillFormData(
    SuggestionType type,
    Suggestion::BackendId backend_id,
    std::optional<SuggestionMetadata> metadata,
    bool is_preview,
    const AutofillTriggerDetails& trigger_details) {
  CHECK(is_preview || metadata);
  // Only address suggestions store the last field types to fill. This is
  // because this is the only use case where filling granularies need to be
  // persisted.
  static constexpr auto kAutofillAddressSuggestions =
      base::MakeFixedFlatSet<SuggestionType>(
          {SuggestionType::kAddressEntry, SuggestionType::kFillFullAddress,
           SuggestionType::kFillFullPhoneNumber, SuggestionType::kFillFullEmail,
           SuggestionType::kFillFullName,
           SuggestionType::kFillEverythingFromAddressProfile});
  const AutofillField* autofill_trigger_field = GetQueriedAutofillField();
  if (autofill_trigger_field && kAutofillAddressSuggestions.contains(type) &&
      !is_preview && metadata->sub_popup_level > 0) {
    // We only update this when the user accepts a subpopup suggestion since the
    // filling granularity doesn't change by accepting a top-level popup
    // suggestion but stays the same.
    last_accepted_address_suggestion_for_address_form_section_
        [autofill_trigger_field->section()] = type;
  }

  mojom::ActionPersistence action_persistence =
      is_preview ? mojom::ActionPersistence::kPreview
                 : mojom::ActionPersistence::kFill;

  PersonalDataManager* pdm = manager_->client().GetPersonalDataManager();
  const AutofillProfile* profile =
      type == SuggestionType::kDevtoolsTestAddressEntry
          ? GetTestAddressByGUID(
                manager_->client().GetTestAddresses(),
                absl::get<Suggestion::Guid>(backend_id).value())
          : pdm->address_data_manager().GetProfileByGUID(
                absl::get<Suggestion::Guid>(backend_id).value());
  if (profile) {
    manager_->FillOrPreviewProfileForm(action_persistence, query_form_,
                                       query_field_, *profile, trigger_details);
  } else if (const CreditCard* credit_card =
                 pdm->payments_data_manager().GetCreditCardByGUID(
                     absl::get<Suggestion::Guid>(backend_id).value())) {
    is_preview
        ? manager_->FillOrPreviewCreditCardForm(
              mojom::ActionPersistence::kPreview, query_form_, query_field_,
              *credit_card, std::u16string(), trigger_details)
        : manager_->AuthenticateThenFillCreditCardForm(
              query_form_, query_field_,
              type == SuggestionType::kVirtualCreditCardEntry
                  ? CreditCard::CreateVirtualCard(*credit_card)
                  : *credit_card,
              trigger_details);
  }
}

void AutofillExternalDelegate::FillPredictionImprovements(
    const Suggestion& suggestion) {
  // Single field filling.
  if (absl::holds_alternative<Suggestion::ValueToFill>(suggestion.payload)) {
    const std::u16string value_to_fill =
        suggestion.GetPayload<Suggestion::ValueToFill>().value();
    manager_->FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::FieldActionType::kReplaceAll,
                                 query_form_, query_field_, value_to_fill,
                                 SuggestionType::kFillPredictionImprovements,
                                 /*field_type_used=*/std::nullopt);
  } else {
    // Full form filling.
    Suggestion::PredictionImprovementsPayload payload =
        suggestion.GetPayload<Suggestion::PredictionImprovementsPayload>();
    manager_->FillOrPreviewFormExperimental(
        mojom::ActionPersistence::kFill,
        FillingProduct::kPredictionImprovements, payload.field_types_to_fill,
        payload.ignorable_skip_reasons, query_form_, query_field_,
        payload.values_to_fill);
  }
}

void AutofillExternalDelegate::InsertDataListValues(
    std::vector<Suggestion>& suggestions) const {
  if (datalist_.empty()) {
    return;
  }

  // Go through the list of autocomplete values and remove them if they are in
  // the list of datalist values.
  auto datalist_values = base::MakeFlatSet<std::u16string_view>(
      datalist_, {}, [](const SelectOption& option) -> std::u16string_view {
        return option.value;
      });
  std::erase_if(suggestions, [&datalist_values](const Suggestion& suggestion) {
    return suggestion.type == SuggestionType::kAutocompleteEntry &&
           datalist_values.contains(suggestion.main_text.value);
  });

  if constexpr (!BUILDFLAG(IS_ANDROID)) {
    // Insert the separator between the datalist and Autofill/Autocomplete
    // values (if there are any).
    if (!suggestions.empty()) {
      suggestions.insert(suggestions.begin(),
                         Suggestion(SuggestionType::kSeparator));
    }
  }

  // Insert the datalist elements at the beginning.
  suggestions.insert(suggestions.begin(), datalist_.size(),
                     Suggestion(SuggestionType::kDatalistEntry));
  for (size_t i = 0; i < datalist_.size(); i++) {
    suggestions[i].main_text =
        Suggestion::Text(datalist_[i].value, Suggestion::Text::IsPrimary(true));
    suggestions[i].labels = {{Suggestion::Text(datalist_[i].text)}};
  }
}

bool AutofillExternalDelegate::IsPaymentsManualFallbackOnNonPaymentsField()
    const {
  if (trigger_source_ ==
      AutofillSuggestionTriggerSource::kManualFallbackPayments) {
    const AutofillField* field = GetQueriedAutofillField();
    return !field || !FieldTypeGroupSet({FieldTypeGroup::kCreditCard,
                                         FieldTypeGroup::kStandaloneCvcField})
                          .contains(field->Type().group());
  }
  return false;
}

AutofillSuggestionTriggerSource
AutofillExternalDelegate::GetReopenTriggerSource() const {
  // Manual fallbacks show suggestions of a specific type. If the Autofill
  // wasn't triggered manually, return
  // `kShowPromptAfterDialogClosedNonManualFallback` to avoid showing other
  // suggestion types.
  return IsAutofillManuallyTriggered(trigger_source_)
             ? trigger_source_
             : AutofillSuggestionTriggerSource::
                   kShowPromptAfterDialogClosedNonManualFallback;
}

void AutofillExternalDelegate::LogRankingContextAfterSuggestionAccepted(
    const Suggestion& accepted_suggestion) {
  CHECK(accepted_suggestion.type == SuggestionType::kCreditCardEntry);
  const Suggestion::Guid& suggestion_guid =
      accepted_suggestion.GetBackendId<Suggestion::Guid>();
  if (suggestion_ranking_context_ &&
      suggestion_ranking_context_->RankingsAreDifferent() &&
      suggestion_ranking_context_->suggestion_rankings_difference_map.contains(
          suggestion_guid)) {
    autofill_metrics::LogAutofillRankingSuggestionDifference(
        suggestion_ranking_context_->suggestion_rankings_difference_map
            .find(suggestion_guid)
            ->second);
  }
}

void AutofillExternalDelegate::DidAcceptAddressSuggestion(
    const Suggestion& suggestion,
    const SuggestionMetadata& metadata) {
  base::UmaHistogramCounts100(
      "Autofill.Suggestion.AcceptanceFieldValueLength.Address",
      query_field_.value().size());
  switch (suggestion.type) {
    case SuggestionType::kAddressEntry:
      autofill_metrics::LogSuggestionAcceptedIndex(
          metadata.row,
          GetFillingProductFromSuggestionType(SuggestionType::kAddressEntry),
          manager_->client().IsOffTheRecord());
      ABSL_FALLTHROUGH_INTENDED;
    case SuggestionType::kFillEverythingFromAddressProfile:
      autofill_metrics::LogFillingMethodUsed(
          FillingMethod::kFullForm, FillingProduct::kAddress,
          /*triggering_field_type_matches_filling_product=*/true);
      FillAutofillFormData(
          suggestion.type, suggestion.GetPayload<Suggestion::BackendId>(),
          metadata, /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      break;
    case SuggestionType::kFillFullAddress:
    case SuggestionType::kFillFullName:
    case SuggestionType::kFillFullEmail:
    case SuggestionType::kFillFullPhoneNumber: {
      FillingMethod filling_method =
          GetFillingMethodFromSuggestionType(suggestion.type);
      autofill_metrics::LogFillingMethodUsed(
          filling_method, FillingProduct::kAddress,
          /*triggering_field_type_matches_filling_product=*/true);
      FillAutofillFormData(
          suggestion.type, suggestion.GetPayload<Suggestion::BackendId>(),
          metadata, /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill =
               GetTargetFieldTypesFromFillingMethod(filling_method)});
      break;
    }
    case SuggestionType::kAddressFieldByFieldFilling:
      FillFieldByFieldFillingSuggestion(suggestion, metadata);
      break;
    case SuggestionType::kEditAddressProfile:
      ShowEditAddressProfileDialog(
          suggestion.GetBackendId<Suggestion::Guid>().value());
      break;
    case SuggestionType::kDeleteAddressProfile:
      ShowDeleteAddressProfileDialog(
          suggestion.GetBackendId<Suggestion::Guid>().value());
      break;
    case SuggestionType::kDevtoolsTestAddressEntry: {
      const AutofillProfile* profile = GetTestAddressByGUID(
          manager_->client().GetTestAddresses(),
          absl::get<Suggestion::Guid>(
              suggestion.GetPayload<Suggestion::BackendId>())
              .value());
      CHECK(profile);
      autofill_metrics::OnDevtoolsTestAddressesAccepted(
          profile->GetInfo(ADDRESS_HOME_COUNTRY, "en-US"));
      FillAutofillFormData(
          suggestion.type, suggestion.GetPayload<Suggestion::BackendId>(),
          metadata, /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      break;
    }
    default:
      NOTREACHED();  // Should be handled elsewhere.
  }
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // The user having accepted an address suggestion on this field, all strikes
  // previously recorded for this field are cleared so that address suggestions
  // can be automatically shown again if needed.
  manager_->client()
      .GetPersonalDataManager()
      ->address_data_manager()
      .ClearStrikesToBlockAddressSuggestions(
          CalculateFormSignature(query_form_),
          CalculateFieldSignatureForField(query_field_), query_form_.url());
#endif
}

void AutofillExternalDelegate::DidAcceptPaymentsSuggestion(
    const Suggestion& suggestion,
    const SuggestionMetadata& metadata) {
  base::UmaHistogramCounts100(
      "Autofill.Suggestion.AcceptanceFieldValueLength.CreditCard",
      query_field_.value().size());
  switch (suggestion.type) {
    case SuggestionType::kCreditCardEntry:
      autofill_metrics::LogSuggestionAcceptedIndex(
          metadata.row,
          GetFillingProductFromSuggestionType(SuggestionType::kCreditCardEntry),
          manager_->client().IsOffTheRecord());
      if (base::FeatureList::IsEnabled(
              features::kAutofillEnableRankingFormulaCreditCards)) {
        LogRankingContextAfterSuggestionAccepted(suggestion);
      }
      FillAutofillFormData(
          suggestion.type, suggestion.GetPayload<Suggestion::BackendId>(),
          metadata, /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      break;
    case SuggestionType::kVirtualCreditCardEntry:
      if (IsPaymentsManualFallbackOnNonPaymentsField()) {
        if (const CreditCard* credit_card =
                manager_->client()
                    .GetPersonalDataManager()
                    ->payments_data_manager()
                    .GetCreditCardByGUID(
                        suggestion.GetBackendId<Suggestion::Guid>().value())) {
          CreditCard virtual_card = CreditCard::CreateVirtualCard(*credit_card);
          manager_->GetCreditCardAccessManager().FetchCreditCard(
              &virtual_card,
              base::BindOnce(
                  &AutofillExternalDelegate::OnVirtualCreditCardFetched,
                  GetWeakPtr()));
        }
      } else {
        // There can be multiple virtual credit cards that all rely on
        // SuggestionType::kVirtualCreditCardEntry as a `type`.
        // In this case, the payload contains the backend id, which is a GUID
        // that identifies the actually chosen credit card.
        FillAutofillFormData(
            suggestion.type, suggestion.GetPayload<Suggestion::BackendId>(),
            metadata, /*is_preview=*/false,
            {.trigger_source =
                 TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      }
      break;
    case SuggestionType::kCreditCardFieldByFieldFilling:
      FillFieldByFieldFillingSuggestion(suggestion, metadata);
      break;
    case SuggestionType::kIbanEntry:
      // User chooses an IBAN suggestion and if it is a local IBAN, full IBAN
      // value will directly populate the IBAN field. In the case of a server
      // IBAN, a request to unmask the IBAN will be sent to the GPay server, and
      // the IBAN value will be filled if the request is successful.
      manager_->client()
          .GetPaymentsAutofillClient()
          ->GetIbanAccessManager()
          ->FetchValue(suggestion.GetPayload<Suggestion::BackendId>(),
                       base::BindOnce(
                           [](base::WeakPtr<AutofillExternalDelegate> delegate,
                              const std::u16string& value) {
                             if (delegate) {
                               delegate->manager_->FillOrPreviewField(
                                   mojom::ActionPersistence::kFill,
                                   mojom::FieldActionType::kReplaceAll,
                                   delegate->query_form_,
                                   delegate->query_field_, value,
                                   SuggestionType::kIbanEntry, IBAN_VALUE);
                             }
                           },
                           GetWeakPtr()));
      manager_->OnSingleFieldSuggestionSelected(suggestion, query_form_,
                                                query_field_);
      break;
    case SuggestionType::kMerchantPromoCodeEntry:
      // User selected an Autocomplete or Merchant Promo Code field, so we fill
      // directly.
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          query_form_, query_field_, suggestion.main_text.value,
          suggestion.type, MERCHANT_PROMO_CODE);
      manager_->OnSingleFieldSuggestionSelected(suggestion, query_form_,
                                                query_field_);
      break;
    case SuggestionType::kSeePromoCodeDetails:
      // Open a new tab and navigate to the offer details page.
      manager_->client()
          .GetPaymentsAutofillClient()
          ->OpenPromoCodeOfferDetailsURL(suggestion.GetPayload<GURL>());
      manager_->OnSingleFieldSuggestionSelected(suggestion, query_form_,
                                                query_field_);
      break;
    case SuggestionType::kShowAccountCards:
      autofill_metrics::LogAutofillShowCardsFromGoogleAccountButtonEventMetric(
          autofill_metrics::ShowCardsFromGoogleAccountButtonEvent::
              kButtonClicked);
      manager_->OnUserAcceptedCardsFromAccountOption();
      break;
    case SuggestionType::kScanCreditCard:
      manager_->client().GetPaymentsAutofillClient()->ScanCreditCard(
          base::BindOnce(&AutofillExternalDelegate::OnCreditCardScanned,
                         GetWeakPtr(),
                         AutofillTriggerSource::kKeyboardAccessory));
      break;
    default:
      NOTREACHED();  // Should be handled elsewhere
  }
  if (base::Contains(shown_suggestion_types_,
                     SuggestionType::kScanCreditCard)) {
    AutofillMetrics::LogScanCreditCardPromptMetric(
        suggestion.type == SuggestionType::kScanCreditCard
            ? AutofillMetrics::SCAN_CARD_ITEM_SELECTED
            : AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED);
  }
}

PlusAddressCallback AutofillExternalDelegate::CreatePlusAddressCallback(
    SuggestionType suggestion_type) {
  return base::BindRepeating(
             [](const std::string& s) { return base::UTF8ToUTF16(s); })
      .Then(CreateSingleFieldFillCallback(suggestion_type, EMAIL_ADDRESS));
}

void AutofillExternalDelegate::DidAcceptCreateNewPlusAddressInlineSuggestion(
    const Suggestion& suggestion) {
  AutofillPlusAddressDelegate* delegate =
      manager_->client().GetPlusAddressDelegate();
  if (!delegate) {
    return;
  }

  base::span<const Suggestion> suggestions =
      manager_->client().GetAutofillSuggestions();
  // TODO(crbug.com/362445807): Change the signature of DidAcceptSuggestion to
  // pass all suggestions and an index of the currently focused one.
  auto it = std::ranges::find(suggestions, suggestion);
  CHECK(it != suggestions.end());

  auto show_affiliation_error = base::BindOnce(
      [](base::WeakPtr<AutofillExternalDelegate> self,
         base::OnceCallback<void(const std::u16string&)> fill_callback,
         std::u16string affiliated_domain,
         std::u16string affiliated_plus_address) {
        if (!self) {
          return;
        }
        base::OnceClosure bound_fill_callback =
            base::BindOnce(std::move(fill_callback), affiliated_plus_address);
        self->manager_->client().ShowPlusAddressAffiliationError(
            std::move(affiliated_domain), std::move(affiliated_plus_address),
            std::move(bound_fill_callback));
      },
      GetWeakPtr(),
      CreateSingleFieldFillCallback(SuggestionType::kCreateNewPlusAddressInline,
                                    /*field_type_used=*/EMAIL_ADDRESS));

  auto show_error = base::BindOnce(
      [](base::WeakPtr<AutofillExternalDelegate> self,
         AutofillClient::PlusAddressErrorDialogType error_dialog_type,
         base::OnceClosure on_accepted) {
        if (!self) {
          return;
        }
        self->manager_->client().ShowPlusAddressError(error_dialog_type,
                                                      std::move(on_accepted));
      },
      GetWeakPtr());

  base::OnceClosure reshow_suggestions = base::BindOnce(
      [](base::WeakPtr<AutofillExternalDelegate> self, FieldGlobalId field) {
        if (!self) {
          return;
        }
        // Manual fallbacks are used as a trigger source to guarantee that a
        // plus address suggestion will show.
        self->manager_->driver().RendererShouldTriggerSuggestions(
            field,
            AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses);
      },
      GetWeakPtr(), query_field_.global_id());

  delegate->OnAcceptedInlineSuggestion(
      manager_->client().GetLastCommittedPrimaryMainFrameOrigin(), suggestions,
      /*current_suggestion_index=*/it - suggestions.begin(),
      CreateUpdateSuggestionsCallback(), CreateHideSuggestionsCallback(),
      CreatePlusAddressCallback(SuggestionType::kCreateNewPlusAddressInline),
      std::move(show_affiliation_error), std::move(show_error),
      std::move(reshow_suggestions));
}

}  // namespace autofill
