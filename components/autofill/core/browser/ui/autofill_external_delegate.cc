// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_external_delegate.h"

#include <stddef.h>

#include <functional>
#include <iterator>
#include <optional>
#include <utility>
#include <variant>

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
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/zip.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/addresses/field_filling_address_util.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_delegate.h"
#include "components/autofill/core/browser/integrators/compose/autofill_compose_delegate.h"
#include "components/autofill/core/browser/integrators/plus_addresses/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/metrics/autofill_in_devtools_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/single_field_fillers/autocomplete/autocomplete_history_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/signatures.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"

namespace autofill {

namespace {

std::optional<AutofillProfile> GetTestAddressByGUID(
    base::span<const AutofillProfile> test_addresses,
    const std::string& guid) {
  if (test_addresses.empty()) {
    return std::nullopt;
  }
  auto it = std::ranges::find(test_addresses, guid, &AutofillProfile::guid);
  if (it == test_addresses.end()) {
    return std::nullopt;
  }
  return *it;
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
    case AutofillSuggestionTriggerSource::kTextFieldValueChanged:
    case AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown:
    case AutofillSuggestionTriggerSource::kOpenTextDataListChooser:
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
    case AutofillSuggestionTriggerSource::kManualFallbackPasswords:
    case AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses:
      // Manual fallbacks are both a suggestion trigger source (e.g. through the
      // context menu) and a trigger source (by selecting a suggestion generated
      // through the context menu).
      return AutofillTriggerSource::kManualFallback;
    case AutofillSuggestionTriggerSource::kAutofillAi:
      return AutofillTriggerSource::kAutofillAi;
  }
  NOTREACHED();
}

// Returns a pointer to the first Suggestion whose GUID matches that of a
// AutofillClient::GetTestAddresses() profile.
const Suggestion* FindTestSuggestion(AutofillClient& client,
                                     base::span<const Suggestion> suggestions,
                                     int index) {
  auto is_test_suggestion = [&client](const Suggestion& suggestion) {
    auto* guid = std::get_if<Suggestion::Guid>(&suggestion.payload);
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

// Loads the AutofillProfile from the personal data manager and returns a copy
// to it if exists or `std::nullopt` otherwise. In case the payload contains a
// non-empty email override, it applies it on the profile before returning it.
std::optional<AutofillProfile> GetProfileFromPayload(
    const PersonalDataManager& pdm,
    const Suggestion::Payload& payload) {
  auto GetProfileFromPersonalDataManager =
      [&pdm](const std::string& guid) -> std::optional<AutofillProfile> {
    if (const AutofillProfile* profile =
            pdm.address_data_manager().GetProfileByGUID(guid)) {
      return *profile;
    }
    return std::nullopt;
  };

  const Suggestion::AutofillProfilePayload& details =
      std::get<Suggestion::AutofillProfilePayload>(payload);
  std::optional<AutofillProfile> profile =
      GetProfileFromPersonalDataManager(details.guid.value());
  if (profile && !details.email_override.empty()) {
    profile->SetRawInfo(EMAIL_ADDRESS, details.email_override);
  }
  return profile;
}

// Used to determine autofill availability for a11y. Presence of a suggestion
// for which this method returns `true` makes screen readers change
// the field announcement to notify users about available autofill options,
// e.g. VoiceOver adds "with autofill menu.".
bool HasAutofillSugestionsForA11y(SuggestionType item_id) {
  switch (item_id) {
    // TODO(crbug.com/374918460): Consider adding other types that can be
    // classified as "providing autofill capabilities".
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kLoyaltyCardEntry:
      return true;
    default:
      return AutofillExternalDelegate::IsAutofillAndFirstLayerSuggestionId(
          item_id);
  }
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
    case SuggestionType::kAddressEntryOnTyping:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kHomeAndWorkAddressEntry:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kSaveAndFillCreditCardEntry:
      // Virtual cards can appear on their own when filling the CVC for a card
      // that a merchant has saved. This indicates there could be Autofill
      // suggestions related to standalone CVC fields.
    case SuggestionType::kVirtualCreditCardEntry:
      return true;
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kAutocompleteEntry:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kCreateNewPlusAddress:
    case SuggestionType::kCreateNewPlusAddressInline:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kFillPassword:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kPlusAddressError:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kSeparator:
    case SuggestionType::kTitle:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kIdentityCredential:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kPendingStateSignin:
      return false;
  }
}

void AutofillExternalDelegate::OnQuery(
    const FormData& form,
    const FormFieldData& field,
    const gfx::Rect& caret_bounds,
    AutofillSuggestionTriggerSource trigger_source,
    bool update_datalist) {
  query_form_ = form;
  query_field_ = field;
  caret_bounds_ = caret_bounds;
  trigger_source_ = trigger_source;
  if (update_datalist) {
    manager_->client().UpdateAutofillDataListValues(
        query_field_.datalist_options());
  }
}

const AutofillField* AutofillExternalDelegate::GetQueriedAutofillField() const {
  return manager_->GetAutofillField(query_form_.global_id(),
                                    query_field_.global_id());
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

bool AutofillExternalDelegate::HasActiveScreenReader() const {
#if BUILDFLAG(IS_IOS)
  // ui::AXPlatform is not supported on iOS. The rendering engine handles
  // a11y internally.
  return false;
#else
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

std::variant<AutofillDriver*, password_manager::PasswordManagerDriver*>
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

  if (std::ranges::any_of(shown_suggestion_types,
                          HasAutofillSugestionsForA11y)) {
    OnAutofillAvailabilityEvent(
        mojom::AutofillSuggestionAvailability::kAutofillAvailable);
  } else {
    // We send autocomplete availability event even though there might be no
    // autocomplete suggestions shown.
    // TODO(crbug.com/315748930): Provide AX event only for autocomplete
    // entries.
    OnAutofillAvailabilityEvent(
        mojom::AutofillSuggestionAvailability::kAutocompleteAvailable);
    if (shown_suggestion_types.contains(SuggestionType::kAutocompleteEntry) &&
        autofill_metrics::ShouldLogAutofillSuggestionShown(trigger_source_)) {
      AutofillMetrics::OnAutocompleteSuggestionsShown();
    }
  }

  manager_->DidShowSuggestions(suggestions, query_form_,
                               query_field_.global_id(),
                               CreateUpdateSuggestionsCallback());
}

void AutofillExternalDelegate::OnSuggestionsHidden() {
  manager_->OnSuggestionsHidden();
}

void AutofillExternalDelegate::DidSelectSuggestion(
    const Suggestion& suggestion) {
  ClearPreviewedForm();

  switch (suggestion.type) {
    case SuggestionType::kUndoOrClear:
#if !BUILDFLAG(IS_IOS)
      manager_->UndoAutofill(mojom::ActionPersistence::kPreview, query_form_,
                             query_field_);
#endif
      break;
    case SuggestionType::kAddressEntry:
    case SuggestionType::kHomeAndWorkAddressEntry:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kDevtoolsTestAddressEntry:
      FillAutofillFormData(
          suggestion.type, suggestion.payload, /*metadata=*/std::nullopt,
          /*is_preview=*/true,
          TriggerSourceFromSuggestionTriggerSource(trigger_source_));
      break;
    case SuggestionType::kAutocompleteEntry:
      manager_->FillOrPreviewField(mojom::ActionPersistence::kPreview,
                                   mojom::FieldActionType::kReplaceAll,
                                   query_form_, query_field_,
                                   suggestion.main_text.value, suggestion.type,
                                   /*field_type_used=*/std::nullopt);
      break;
    case SuggestionType::kIbanEntry:
      // Always shows the masked IBAN value as the preview of the suggestion.
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kPreview,
          mojom::FieldActionType::kReplaceAll, query_form_, query_field_,
          suggestion.labels.empty() ? suggestion.main_text.value
                                    : suggestion.labels[0][0].value,
          suggestion.type, IBAN_VALUE);
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
      CHECK(suggestion.field_by_field_filling_type_used);
      if (std::optional<AutofillProfile> profile =
              GetProfileFromPayload(manager_->client().GetPersonalDataManager(),
                                    suggestion.payload)) {
        PreviewAddressFieldByFieldFillingSuggestion(*profile, suggestion);
      }
      break;
    case SuggestionType::kVirtualCreditCardEntry:
      FillAutofillFormData(
          suggestion.type, suggestion.payload, /*metadata=*/std::nullopt,
          /*is_preview=*/true,
          TriggerSourceFromSuggestionTriggerSource(trigger_source_));
      break;
    case SuggestionType::kFillAutofillAi:
      if (EntityDataManager* edm = manager_->client().GetEntityDataManager()) {
        if (base::optional_ref<const EntityInstance> entity =
                edm->GetEntityInstance(
                    suggestion.GetPayload<Suggestion::AutofillAiPayload>()
                        .guid)) {
          manager_->FillOrPreviewForm(mojom::ActionPersistence::kPreview,
                                      query_form_, query_field_.global_id(),
                                      &*entity,
                                      AutofillTriggerSource::kAutofillAi);
        }
      }
      break;
    case SuggestionType::kAddressEntryOnTyping:
      CHECK(suggestion.field_by_field_filling_type_used);
      if (std::optional<AutofillProfile> profile =
              GetProfileFromPayload(manager_->client().GetPersonalDataManager(),
                                    suggestion.payload)) {
        PreviewAddressFieldByFieldFillingSuggestion(*profile, suggestion);
      }
      break;
    case SuggestionType::kIdentityCredential: {
      VerifiedProfile profile =
          suggestion.GetPayload<Suggestion::IdentityCredentialPayload>().fields;

      manager_->FillOrPreviewForm(
          mojom::ActionPersistence::kPreview, query_form_,
          query_field_.global_id(), &profile,
          TriggerSourceFromSuggestionTriggerSource(trigger_source_));
      break;
    }
    case SuggestionType::kLoyaltyCardEntry:
      // Always shows the masked loyalty card value as the preview of the
      // suggestion.
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kPreview,
          mojom::FieldActionType::kReplaceAll, query_form_, query_field_,
          suggestion.main_text.value, suggestion.type, LOYALTY_MEMBERSHIP_ID);
      break;
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kCreateNewPlusAddress:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kPlusAddressError:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kBnplEntry:
      break;
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kPendingStateSignin:
      NOTREACHED();  // Should be handled elsewhere.
  }
}

void AutofillExternalDelegate::DidAcceptSuggestion(
    const Suggestion& suggestion,
    const SuggestionMetadata& metadata) {
  CHECK(suggestion.IsAcceptable());
  base::UmaHistogramEnumeration("Autofill.Suggestions.AcceptedType",
                                suggestion.type);

  switch (suggestion.type) {
    case SuggestionType::kAddressEntry:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kHomeAndWorkAddressEntry:
      DidAcceptAddressSuggestion(suggestion, metadata);
      break;
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kBnplEntry:
      DidAcceptPaymentsSuggestion(suggestion, metadata);
      break;
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
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
      manager_->OnSingleFieldSuggestionSelected(
          suggestion, query_form_.global_id(), query_field_.global_id());
      break;
    case SuggestionType::kFillExistingPlusAddress:
      if (AutofillPlusAddressDelegate* plus_address_delegate =
              manager_->client().GetPlusAddressDelegate()) {
        plus_address_delegate->RecordAutofillSuggestionEvent(
            AutofillPlusAddressDelegate::SuggestionEvent::
                kExistingPlusAddressChosen);
        plus_address_delegate->DidFillPlusAddress();
        if (trigger_source_ ==
            AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses) {
          manager_->client().TriggerPlusAddressUserPerceptionSurvey(
              plus_addresses::hats::SurveyType::
                  kFilledPlusAddressViaManualFallack);
        } else {
          manager_->client().TriggerPlusAddressUserPerceptionSurvey(
              plus_addresses::hats::SurveyType::kDidChoosePlusAddressOverEmail);
        }
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
          trigger_source_ ==
              AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses,
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
            AutofillComposeDelegate::UiEntryPoint::kAutofillPopup);
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
    case SuggestionType::kFillAutofillAi:
      if (EntityDataManager* edm = manager_->client().GetEntityDataManager()) {
        if (base::optional_ref<const EntityInstance> entity =
                edm->GetEntityInstance(
                    suggestion.GetPayload<Suggestion::AutofillAiPayload>()
                        .guid)) {
          manager_->FillOrPreviewForm(mojom::ActionPersistence::kFill,
                                      query_form_, query_field_.global_id(),
                                      &*entity,
                                      AutofillTriggerSource::kAutofillAi);
        }
      }
      break;
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kMixedFormMessage:
      // If the selected element is a warning we don't want to do anything.
      break;
    case SuggestionType::kAddressEntryOnTyping:
      CHECK(suggestion.field_by_field_filling_type_used);
      if (std::optional<AutofillProfile> profile =
              GetProfileFromPayload(manager_->client().GetPersonalDataManager(),
                                    suggestion.payload)) {
        FillAddressFieldByFieldFillingSuggestion(*profile, suggestion,
                                                 metadata);
        autofill_metrics::LogAddressAutofillOnTypingSuggestionAccepted(
            suggestion.field_by_field_filling_type_used.value(),
            GetQueriedAutofillField());
      }
      break;
    case SuggestionType::kIdentityCredential: {
      if (const IdentityCredentialDelegate* identity_credential_delegate =
              manager_->client().GetIdentityCredentialDelegate()) {
        identity_credential_delegate->NotifySuggestionAccepted(
            suggestion, /*show_modal=*/true,
            base::BindOnce(
                [](base::WeakPtr<AutofillExternalDelegate> delegate,
                   const Suggestion& suggestion, bool accepted) {
                  if (!delegate || !accepted) {
                    return;
                  }

                  VerifiedProfile profile =
                      suggestion
                          .GetPayload<Suggestion::IdentityCredentialPayload>()
                          .fields;
                  delegate->manager_->FillOrPreviewForm(
                      mojom::ActionPersistence::kFill, delegate->query_form_,
                      delegate->query_field_.global_id(), &profile,
                      TriggerSourceFromSuggestionTriggerSource(
                          delegate->trigger_source_));
                },
                GetWeakPtr(), suggestion));
      }
      break;
    }
    case SuggestionType::kLoyaltyCardEntry: {
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          query_form_, query_field_, suggestion.main_text.value,
          SuggestionType::kLoyaltyCardEntry, LOYALTY_MEMBERSHIP_ID);
      break;
    }
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kPendingStateSignin:
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
    default:
      NOTREACHED();
  }
}

bool AutofillExternalDelegate::RemoveSuggestion(const Suggestion& suggestion) {
  switch (suggestion.type) {
    // These SuggestionTypes are various types which can appear in the first
    // level suggestion to fill an address or credit card field.
    case SuggestionType::kAddressEntry:
    case SuggestionType::kHomeAndWorkAddressEntry:
    case SuggestionType::kAddressFieldByFieldFilling: {
      const std::string guid =
          std::get<Suggestion::AutofillProfilePayload>(suggestion.payload)
              .guid.value();
      if (AddressDataManager& adm = manager_->client()
                                        .GetPersonalDataManager()
                                        .address_data_manager();
          auto* profile = adm.GetProfileByGUID(guid)) {
        switch (profile->record_type()) {
          case AutofillProfile::RecordType::kLocalOrSyncable:
          case AutofillProfile::RecordType::kAccount:
            adm.RemoveProfile(guid);
            return true;
          case AutofillProfile::RecordType::kAccountHome:
          case AutofillProfile::RecordType::kAccountWork:
            // Home and Work profiles are read-only and therefore cannot be
            // deleted.
            break;
        }
      }
      return false;
    }
    case SuggestionType::kCreditCardEntry: {
      const std::string guid =
          std::get<Suggestion::Guid>(suggestion.payload).value();
      if (PaymentsDataManager& pdm = manager_->client()
                                         .GetPersonalDataManager()
                                         .payments_data_manager();
          const CreditCard* credit_card = pdm.GetCreditCardByGUID(guid)) {
        // Server cards cannot be deleted from within Chrome.
        if (CreditCard::IsLocalCard(credit_card)) {
          pdm.DeleteLocalCreditCards({*credit_card});
          return true;
        }
      }
      return false;
    }
    case SuggestionType::kAutocompleteEntry:
      manager_->client()
          .GetSingleFieldFillRouter()
          .OnRemoveCurrentSingleFieldSuggestion(
              query_field_.name(), suggestion.main_text.value, suggestion.type);
      return true;
    case SuggestionType::kAddressEntryOnTyping:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kCreateNewPlusAddress:
    case SuggestionType::kCreateNewPlusAddressInline:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kPlusAddressError:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kIdentityCredential:
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
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kLoyaltyCardEntry:
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

void AutofillExternalDelegate::OnCreditCardScanned(const CreditCard& card) {
  manager_->FillOrPreviewForm(mojom::ActionPersistence::kFill, query_form_,
                              query_field_.global_id(), &card,
                              AutofillTriggerSource::kScanCreditCard);
}

void AutofillExternalDelegate::PreviewAddressFieldByFieldFillingSuggestion(
    const AutofillProfile& profile,
    const Suggestion& suggestion) {
  const auto& [filling_value, filling_type] = GetFillingValueAndTypeForProfile(
      profile, manager_->client().GetAppLocale(),
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
  const auto& [filling_value, filling_type] = GetFillingValueAndTypeForProfile(
      profile, manager_->client().GetAppLocale(),
      AutofillType(*suggestion.field_by_field_filling_type_used), query_field_,
      manager_->client().GetAddressNormalizer());
  if (!filling_value.empty()) {
    manager_->FillOrPreviewField(
        mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
        query_form_, query_field_, filling_value, suggestion.type,
        suggestion.field_by_field_filling_type_used);
    if (suggestion.type == SuggestionType::kAddressFieldByFieldFilling) {
      // Ensure that `SuggestionType::kAddressEntryOnTyping` do not (at least
      // yet) affect key metrics.
      manager_->OnDidFillAddressFormFillingSuggestion(
          profile, query_form_.global_id(), query_field_.global_id(),
          TriggerSourceFromSuggestionTriggerSource(trigger_source_));
    } else if (suggestion.type == SuggestionType::kAddressEntryOnTyping) {
      manager_->OnDidFillAddressOnTypingSuggestion(
          query_field_.global_id(), filling_value,
          *suggestion.field_by_field_filling_type_used,
          /*profile_last_time_used*/ profile.guid());
    }
  }
}

void AutofillExternalDelegate::FillAutofillFormData(
    SuggestionType type,
    const Suggestion::Payload& payload,
    std::optional<SuggestionMetadata> metadata,
    bool is_preview,
    AutofillTriggerSource trigger_source) {
  CHECK(is_preview || metadata);
  mojom::ActionPersistence action_persistence =
      is_preview ? mojom::ActionPersistence::kPreview
                 : mojom::ActionPersistence::kFill;

  PersonalDataManager& pdm = manager_->client().GetPersonalDataManager();
  if (const Suggestion::AutofillProfilePayload* profile_payload =
          std::get_if<Suggestion::AutofillProfilePayload>(&payload)) {
    std::optional<AutofillProfile> profile =
        type == SuggestionType::kDevtoolsTestAddressEntry
            ? GetTestAddressByGUID(manager_->client().GetTestAddresses(),
                                   profile_payload->guid.value())
            : GetProfileFromPayload(pdm, payload);
    if (profile) {
      manager_->FillOrPreviewForm(action_persistence, query_form_,
                                  query_field_.global_id(), &*profile,
                                  trigger_source);
    }
    return;
  }
  if (const CreditCard* credit_card =
          pdm.payments_data_manager().GetCreditCardByGUID(
              std::get<Suggestion::Guid>(payload).value())) {
    const CreditCard& card_to_fill =
        !is_preview && type == SuggestionType::kVirtualCreditCardEntry
            ? CreditCard::CreateVirtualCard(*credit_card)
            : *credit_card;
    manager_->FillOrPreviewForm(action_persistence, query_form_,
                                query_field_.global_id(), &card_to_fill,
                                trigger_source);
  }
}

void AutofillExternalDelegate::InsertDataListValues(
    std::vector<Suggestion>& suggestions) const {
  const std::vector<SelectOption>& datalist = query_field_.datalist_options();
  if (datalist.empty()) {
    return;
  }

  AutofillMetrics::LogDataListSuggestionsInserted();
  // Go through the list of autocomplete values and remove them if they are in
  // the list of datalist values.
  auto datalist_values = base::MakeFlatSet<std::u16string_view>(
      datalist, {}, [](const SelectOption& option) -> std::u16string_view {
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
  suggestions.insert(suggestions.begin(), datalist.size(),
                     Suggestion(SuggestionType::kDatalistEntry));
  for (auto [suggestion, list_entry] : base::zip(suggestions, datalist)) {
    suggestion.main_text =
        Suggestion::Text(list_entry.value, Suggestion::Text::IsPrimary(true));
    suggestion.labels = {{Suggestion::Text(list_entry.text)}};
  }
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
      accepted_suggestion.GetPayload<Suggestion::Guid>();
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
    case SuggestionType::kAddressEntry: {
      autofill_metrics::LogSuggestionAcceptedIndex(
          metadata.row,
          GetFillingProductFromSuggestionType(SuggestionType::kAddressEntry),
          manager_->client().IsOffTheRecord());
      const bool email_and_plus_address_shown = [this] {
        const AutofillField* autofill_trigger_field = GetQueriedAutofillField();
        const bool triggered_on_email_field =
            autofill_trigger_field &&
            autofill_trigger_field->Type().group() == FieldTypeGroup::kEmail;
        // Email suggestions don't have a separate suggestion type. Check that
        // the suggestions are triggered on an email field and that the popup
        // contains a plus address filling suggestion as well.
        return triggered_on_email_field &&
               base::Contains(shown_suggestion_types_,
                              SuggestionType::kFillExistingPlusAddress);
      }();
      if (AutofillPlusAddressDelegate* plus_address_delegate =
              manager_->client().GetPlusAddressDelegate();
          plus_address_delegate && email_and_plus_address_shown) {
        manager_->client().TriggerPlusAddressUserPerceptionSurvey(
            plus_addresses::hats::SurveyType::kDidChooseEmailOverPlusAddress);
      }
      FillAutofillFormData(
          suggestion.type, suggestion.payload, metadata,
          /*is_preview=*/false,
          TriggerSourceFromSuggestionTriggerSource(trigger_source_));
      break;
    }
    case SuggestionType::kAddressFieldByFieldFilling:
      CHECK(suggestion.field_by_field_filling_type_used);
      if (std::optional<AutofillProfile> profile =
              GetProfileFromPayload(manager_->client().GetPersonalDataManager(),
                                    suggestion.payload)) {
        FillAddressFieldByFieldFillingSuggestion(*profile, suggestion,
                                                 metadata);
      }
      break;
    case SuggestionType::kDevtoolsTestAddressEntry: {
      const std::optional<AutofillProfile> profile = GetTestAddressByGUID(
          manager_->client().GetTestAddresses(),
          suggestion.GetPayload<Suggestion::AutofillProfilePayload>()
              .guid.value());
      CHECK(profile);
      autofill_metrics::OnDevtoolsTestAddressesAccepted(
          profile->GetInfo(ADDRESS_HOME_COUNTRY, "en-US"));
      FillAutofillFormData(
          suggestion.type, suggestion.payload, metadata,
          /*is_preview=*/false,
          TriggerSourceFromSuggestionTriggerSource(trigger_source_));
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
      .address_data_manager()
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
          suggestion.type, suggestion.payload, metadata,
          /*is_preview=*/false,
          TriggerSourceFromSuggestionTriggerSource(trigger_source_));
      break;
    case SuggestionType::kVirtualCreditCardEntry:
      // There can be multiple virtual credit cards that all rely on
      // SuggestionType::kVirtualCreditCardEntry as a `type`.
      // In this case, the payload contains the backend id, which is a GUID
      // that identifies the actually chosen credit card.
      FillAutofillFormData(
          suggestion.type, suggestion.payload, metadata, /*is_preview=*/false,
          TriggerSourceFromSuggestionTriggerSource(trigger_source_));
      break;
    case SuggestionType::kIbanEntry:
      // User chooses an IBAN suggestion and if it is a local IBAN, full IBAN
      // value will directly populate the IBAN field. In the case of a server
      // IBAN, a request to unmask the IBAN will be sent to the GPay server, and
      // the IBAN value will be filled if the request is successful.
      manager_->client()
          .GetPaymentsAutofillClient()
          ->GetIbanAccessManager()
          ->FetchValue(suggestion.payload,
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
      manager_->OnSingleFieldSuggestionSelected(
          suggestion, query_form_.global_id(), query_field_.global_id());
      break;
    case SuggestionType::kMerchantPromoCodeEntry:
      // User selected an Autocomplete or Merchant Promo Code field, so we fill
      // directly.
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          query_form_, query_field_, suggestion.main_text.value,
          suggestion.type, MERCHANT_PROMO_CODE);
      manager_->OnSingleFieldSuggestionSelected(
          suggestion, query_form_.global_id(), query_field_.global_id());
      break;
    case SuggestionType::kSeePromoCodeDetails:
      // Open a new tab and navigate to the offer details page.
      manager_->client()
          .GetPaymentsAutofillClient()
          ->OpenPromoCodeOfferDetailsURL(suggestion.GetPayload<GURL>());
      manager_->OnSingleFieldSuggestionSelected(
          suggestion, query_form_.global_id(), query_field_.global_id());
      break;
    case SuggestionType::kSaveAndFillCreditCardEntry:
      manager_->client()
          .GetPaymentsAutofillClient()
          ->ShowCreditCardSaveAndFillDialog();
      break;
    case SuggestionType::kScanCreditCard:
      manager_->client().GetPaymentsAutofillClient()->ScanCreditCard(
          base::BindOnce(&AutofillExternalDelegate::OnCreditCardScanned,
                         GetWeakPtr()));
      break;
    case SuggestionType::kBnplEntry: {
      CHECK(suggestion.GetPayload<Suggestion::PaymentsPayload>()
                .extracted_amount_in_micros.has_value());
      payments::BnplManager* bnpl_manager = manager_->GetPaymentsBnplManager();
      CHECK(bnpl_manager);

      bnpl_manager->OnDidAcceptBnplSuggestion(
          /*final_checkout_amount=*/suggestion
              .GetPayload<Suggestion::PaymentsPayload>()
              .extracted_amount_in_micros.value(),
          base::BindOnce(
              [](base::WeakPtr<AutofillExternalDelegate> delegate,
                 const CreditCard& card) {
                if (delegate) {
                  delegate->manager_->FillOrPreviewForm(
                      mojom::ActionPersistence::kFill, delegate->query_form_,
                      delegate->query_field_.global_id(), &card,
                      AutofillTriggerSource::kPopup);
                }
              },
              GetWeakPtr()));
      break;
    }
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

PlusAddressCallback AutofillExternalDelegate::CreateInlinePlusAddressCallback(
    SuggestionType suggestion_type) {
  return CreatePlusAddressCallback(suggestion_type)
      .Then(base::BindRepeating(
          [](base::WeakPtr<AutofillClient> client, bool is_manual_fallback) {
            if (is_manual_fallback) {
              client->TriggerPlusAddressUserPerceptionSurvey(
                  plus_addresses::hats::SurveyType::
                      kCreatedPlusAddressViaManualFallback);
            } else if (client->GetPlusAddressDelegate()
                           ->GetPlusAddressesCount() > 2) {
              client->TriggerPlusAddressUserPerceptionSurvey(
                  plus_addresses::hats::SurveyType::
                      kCreatedMultiplePlusAddresses);
            }
          },
          manager_->client().GetWeakPtr(),
          trigger_source_ ==
              AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses));
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
      CreateInlinePlusAddressCallback(
          SuggestionType::kCreateNewPlusAddressInline),
      std::move(show_affiliation_error), std::move(show_error),
      std::move(reshow_suggestions));
}

}  // namespace autofill
