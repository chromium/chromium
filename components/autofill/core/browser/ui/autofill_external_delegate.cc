// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_external_delegate.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <iterator>
#include <optional>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
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
#include "components/autofill/core/browser/autofill_ai_form_rationalization.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/addresses/field_filling_address_util.h"
#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager.h"
#include "components/autofill/core/browser/integrators/compose/autofill_compose_delegate.h"
#include "components/autofill/core/browser/integrators/identity_credential/identity_credential_delegate.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_suggestion.h"
#include "components/autofill/core/browser/integrators/plus_addresses/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/metrics/autofill_in_devtools_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/loyalty_cards_metrics.h"
#include "components/autofill/core/browser/metrics/payments/save_and_fill_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/save_and_fill_manager.h"
#include "components/autofill/core/browser/single_field_fillers/autocomplete/autocomplete_history_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/plus_address_survey_type.h"
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

// Returns a pointer to the first Suggestion whose GUID matches that of a
// AutofillClient::GetTestAddresses() profile.
const Suggestion* FindTestSuggestion(AutofillClient& client,
                                     base::span<const Suggestion> suggestions,
                                     int index) {
  auto is_test_suggestion = [&client](const Suggestion& suggestion) {
    auto* guid = std::get_if<Suggestion::Guid>(&suggestion.payload);
    base::span<const AutofillProfile> test_addresses =
        client.GetTestAddresses();

    return guid && std::ranges::contains(test_addresses, guid->value(),
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

// Loads the AutofillProfile from the address data manager and returns a copy
// of it if exists or `std::nullopt` otherwise. In case the payload contains a
// non-empty email override, it applies it to the profile before returning it.
std::optional<AutofillProfile> GetProfileFromPayload(
    const AddressDataManager& adm,
    const Suggestion::AutofillProfilePayload& payload) {
  auto GetProfileFromAddressDataManager =
      [&adm](const std::string& guid) -> std::optional<AutofillProfile> {
    if (const AutofillProfile* profile = adm.GetProfileByGUID(guid)) {
      return *profile;
    }
    return std::nullopt;
  };

  std::optional<AutofillProfile> profile =
      GetProfileFromAddressDataManager(payload.guid.value());
  if (profile && !payload.email_override.empty()) {
    profile->SetRawInfo(EMAIL_ADDRESS, payload.email_override);
  }
  return profile;
}

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
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kSaveAndFillCreditCardEntry:
      // Virtual cards can appear on their own when filling the CVC for a card
      // that a merchant has saved. This indicates there could be Autofill
      // suggestions related to standalone CVC fields.
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kOneTimePasswordEntry:
      return true;
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kAutocompleteEntry:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kFillPassword:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageAutofillAiIdentityDocs:
    case SuggestionType::kManageAutofillAiTravel:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kPasswordFieldByFieldFilling:
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
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kAtMemorySearchResult:
    case SuggestionType::kBnplFootnote:
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

const AutofillField* AutofillExternalDelegate::GetQueriedField() const {
  return GetQueriedFormAndField().second;
}

std::pair<const FormStructure*, const AutofillField*>
AutofillExternalDelegate::GetQueriedFormAndField() const {
  const FormStructure* form_structure =
      manager_->FindCachedFormById(query_form_.global_id());
  if (!form_structure) {
    return {nullptr, nullptr};
  }
  return {form_structure,
          form_structure->GetFieldById(query_field_.global_id())};
}

AutofillTriggerSource AutofillExternalDelegate::GetTriggerSource() const {
  return TriggerSourceFromSuggestionTriggerSource(trigger_source_);
}

void AutofillExternalDelegate::OnSuggestionsReturned(
    FieldGlobalId field_id,
    const std::vector<Suggestion>& input_suggestions) {
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
      input_suggestions, trigger_source_,
      /*is_update=*/false, AutofillSuggestionsIgnoreFocusLoss(false));
}

std::optional<AutofillProfile>
AutofillExternalDelegate::GetProfileFromAddressSuggestion(
    const Suggestion& suggestion) const {
  return GetProfileFromPayload(
      manager_->client().GetPersonalDataManager().address_data_manager(),
      std::get<Suggestion::AutofillProfilePayload>(suggestion.payload));
}

base::optional_ref<const EntityInstance>
AutofillExternalDelegate::GetEntityInstance(
    const Suggestion& suggestion) const {
  EntityDataManager* edm = manager_->client().GetEntityDataManager();
  if (!edm) {
    return std::nullopt;
  }
  return edm->GetEntityInstance(
      suggestion.GetPayload<Suggestion::AutofillAiPayload>().guid);
}

void AutofillExternalDelegate::AttemptToDisplayAutofillSuggestions(
    std::vector<Suggestion> suggestions,
    AutofillSuggestionTriggerSource trigger_source,
    bool is_update,
    AutofillSuggestionsIgnoreFocusLoss ignore_focus_loss) {
  CHECK(!*ignore_focus_loss || is_update)
      << "Ignoring focus loss is only supported for updates";
  PossiblyRemoveAutofillWarnings(suggestions);
  // If anything else is added to modify the values after inserting the data
  // list, AutofillPopupControllerImpl::UpdateDataListValues will need to be
  // updated to match.
  InsertDataListValues(suggestions);

  // TODO(crbug.com/362630793): Try to eliminate this state. The controller
  // should be the one that knows about what suggestions were shown and passes
  // it on, not AED.
  trigger_source_ = trigger_source;

  shown_suggestion_types_.clear();
  for (const Suggestion& suggestion : suggestions) {
    shown_suggestion_types_.push_back(suggestion.type);
  }

  if (suggestions.empty()) {
    OnAutofillAvailabilityEvent(
        mojom::AutofillSuggestionAvailability::kNoSuggestions);
    // No suggestions, any popup currently showing is obsolete.
    if (!base::FeatureList::IsEnabled(
            features::kAutofillAndroidKeyboardAccessoryDynamicPositioning)) {
      manager_->client().HideAutofillSuggestions(
          SuggestionHidingReason::kNoSuggestions);
      return;
    }
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
        suggestions, GetMainFillingProduct(), trigger_source_,
        ignore_focus_loss);
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
            std::move(suggestions), trigger_source,
            /*is_update=*/true, AutofillSuggestionsIgnoreFocusLoss(false));
      },
      GetWeakPtr(), *session_id);
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
      manager_->UndoAutofill(mojom::ActionPersistence::kPreview, query_form_,
                             query_field_);
      break;
    case SuggestionType::kAddressEntry:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kDevtoolsTestAddressEntry:
      AutofillForm(suggestion.type, suggestion.payload,
                   /*metadata=*/std::nullopt,
                   /*is_preview=*/true, GetTriggerSource());
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
    case SuggestionType::kAddressFieldByFieldFilling:
      CHECK(suggestion.field_by_field_filling_type_used);
      if (std::optional<AutofillProfile> profile =
              GetProfileFromAddressSuggestion(suggestion)) {
        PreviewAddressFieldByFieldFillingSuggestion(*profile, suggestion);
      }
      break;
    case SuggestionType::kVirtualCreditCardEntry:
      AutofillForm(suggestion.type, suggestion.payload,
                   /*metadata=*/std::nullopt,
                   /*is_preview=*/true, GetTriggerSource());
      break;
    case SuggestionType::kFillAutofillAi:
      if (base::optional_ref<const EntityInstance> entity =
              GetEntityInstance(suggestion)) {
        manager_->FillOrPreviewForm(mojom::ActionPersistence::kPreview,
                                    query_form_, query_field_.global_id(),
                                    entity.as_ptr(), GetTriggerSource());
      }
      break;
    case SuggestionType::kAddressEntryOnTyping:
      CHECK(suggestion.field_by_field_filling_type_used);
      if (std::optional<AutofillProfile> profile =
              GetProfileFromAddressSuggestion(suggestion)) {
        PreviewAddressFieldByFieldFillingSuggestion(*profile, suggestion);
      }
      break;
    case SuggestionType::kIdentityCredential: {
      VerifiedProfile profile =
          suggestion.GetPayload<Suggestion::IdentityCredentialPayload>().fields;
      manager_->FillOrPreviewForm(mojom::ActionPersistence::kPreview,
                                  query_form_, query_field_.global_id(),
                                  &profile, GetTriggerSource());
      break;
    }
    case SuggestionType::kLoyaltyCardEntry:
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kPreview,
          mojom::FieldActionType::kReplaceAll, query_form_, query_field_,
          suggestion.main_text.value, suggestion.type, LOYALTY_MEMBERSHIP_ID);
      break;
    case SuggestionType::kAtMemorySearchResult:
      // TODO(crbug.com/481976778): Preview @memory search result
      break;
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
      manager_->DelegateSelectToPasswordManager(suggestion, query_field_);
      break;
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageAutofillAiIdentityDocs:
    case SuggestionType::kManageAutofillAiTravel:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kBnplEntry:
    // So far OTP suggestions are only available on Android, so no preview
    // is needed. This needs to be changed once Desktop suggestions and UI
    // are implemented.
    case SuggestionType::kOneTimePasswordEntry:
      break;
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kBnplFootnote:
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
    case SuggestionType::kManageAutofillAiIdentityDocs:
    case SuggestionType::kManageAutofillAiTravel:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kManagePlusAddress: {
      manager_->client().ShowAutofillSettings(suggestion.type);
      break;
    }
    case SuggestionType::kUndoOrClear:
      manager_->UndoAutofill(mojom::ActionPersistence::kFill, query_form_,
                             query_field_);
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
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeSavedStateNotification:
      if (AutofillComposeDelegate* delegate =
              manager_->client().GetComposeDelegate()) {
        delegate->OpenCompose(
            manager_->driver(), query_field_.global_id(),
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
      // Autofill AI is responsible for hiding the popup since it may keep it
      // open longer during reauth and server fetching.
      FillAutofillAiFormAndHidePopup(suggestion);
      return;
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kMixedFormMessage:
      // If the selected element is a warning we don't want to do anything.
      break;
    case SuggestionType::kAddressEntryOnTyping:
      CHECK(suggestion.field_by_field_filling_type_used);
      if (std::optional<AutofillProfile> profile =
              GetProfileFromAddressSuggestion(suggestion)) {
        FillAddressFieldByFieldFillingSuggestion(*profile, suggestion,
                                                 metadata);
        autofill_metrics::LogAddressAutofillOnTypingSuggestionAccepted(
            suggestion.field_by_field_filling_type_used.value(),
            GetQueriedField());
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
      const ValuablesDataManager& vdm =
          CHECK_DEREF(manager_->client().GetValuablesDataManager());
      const std::string guid =
          std::get<Suggestion::Guid>(suggestion.payload).value();
      if (std::optional<LoyaltyCard> loyalty_card =
              vdm.GetLoyaltyCardById(ValuableId(guid))) {
        manager_->LogAndRecordLoyaltyCardFill(
            *loyalty_card, query_form_.global_id(), query_field_.global_id());
      }
      break;
    }
    case SuggestionType::kAllLoyaltyCardsEntry: {
      manager_->touch_to_fill_delegate()->ShowTouchToFillForAllLoyaltyCards(
          query_form_, query_field_);
      break;
    }
    case SuggestionType::kOneTimePasswordEntry: {
      auto [form_structure, autofill_field] = GetQueriedFormAndField();
      if (!form_structure || !autofill_field) {
        break;
      }
      OtpFillData otp_fill_data = CreateFillDataForOtpSuggestion(
          *form_structure, *autofill_field, suggestion.main_text.value);
      manager_->FillOrPreviewForm(mojom::ActionPersistence::kFill, query_form_,
                                  query_field_.global_id(), &otp_fill_data,
                                  GetTriggerSource());
      break;
    }
    case SuggestionType::kAtMemorySearchResult:
      // TODO(crbug.com/481976778): Fill @memory search result
      break;
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
      manager_->DelegateAcceptToPasswordManager(suggestion, metadata,
                                                query_field_);
      break;
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kBnplFootnote:
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
    default:
      NOTREACHED();
  }
}

bool AutofillExternalDelegate::RemoveSuggestion(const Suggestion& suggestion) {
  switch (suggestion.type) {
    // These SuggestionTypes are various types which can appear in the first
    // level suggestion to fill an address or credit card field.
    case SuggestionType::kAddressEntry:
    case SuggestionType::kAddressFieldByFieldFilling: {
      const std::string guid =
          std::get<Suggestion::AutofillProfilePayload>(suggestion.payload)
              .guid.value();
      if (AddressDataManager& adm = manager_->client()
                                        .GetPersonalDataManager()
                                        .address_data_manager();
          adm.GetProfileByGUID(guid)) {
        adm.RemoveProfile(guid);
        return true;
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
    case SuggestionType::kManageAutofillAiIdentityDocs:
    case SuggestionType::kManageAutofillAiTravel:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kAllLoyaltyCardsEntry:
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
    case SuggestionType::kOneTimePasswordEntry:
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kAtMemorySearchResult:
    case SuggestionType::kBnplFootnote:
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
          GetTriggerSource());
    } else if (suggestion.type == SuggestionType::kAddressEntryOnTyping) {
      manager_->OnDidFillAddressOnTypingSuggestion(
          query_field_.global_id(), filling_value,
          *suggestion.field_by_field_filling_type_used,
          /*profile_last_time_used*/ profile.guid());
    }
  }
}

void AutofillExternalDelegate::AutofillForm(
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
            : GetProfileFromPayload(pdm.address_data_manager(),
                                    *profile_payload);
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

void AutofillExternalDelegate::DidAcceptAddressSuggestion(
    const Suggestion& suggestion,
    const SuggestionMetadata& metadata) {
  base::UmaHistogramCounts100(
      "Autofill.Suggestion.AcceptanceFieldValueLength.Address",
      query_field_.value().size());
  autofill_metrics::LogSuggestionAcceptedIndex(
      metadata.row, FillingProduct::kAddress,
      manager_->client().IsOffTheRecord());
  switch (suggestion.type) {
    case SuggestionType::kAddressEntry: {
      const AutofillField* autofill_trigger_field = GetQueriedField();
      const ValuablesDataManager* vdm =
          manager_->client().GetValuablesDataManager();

      if (autofill_trigger_field &&
          autofill_trigger_field->Type().GetLoyaltyCardType() ==
              EMAIL_OR_LOYALTY_MEMBERSHIP_ID &&
          vdm && !vdm->GetLoyaltyCards().empty()) {
        LogEmailOrLoyaltyCardSuggestionAccepted(
            autofill_metrics::AutofillEmailOrLoyaltyCardAcceptanceMetricValue::
                kEmailSelected);
      }

      // Email suggestions don't have a separate suggestion type. Check that
      // the suggestions are triggered on an email field and that the popup
      // contains a plus address filling suggestion as well.
      const bool email_and_plus_address_shown =
          autofill_trigger_field &&
          autofill_trigger_field->Type().GetGroups().contains(
              FieldTypeGroup::kEmail) &&
          std::ranges::contains(shown_suggestion_types_,
                                SuggestionType::kFillExistingPlusAddress);
      if (const AutofillPlusAddressDelegate* plus_address_delegate =
              manager_->client().GetPlusAddressDelegate();
          plus_address_delegate && email_and_plus_address_shown) {
        manager_->client().TriggerPlusAddressUserPerceptionSurvey(
            plus_addresses::hats::SurveyType::kDidChooseEmailOverPlusAddress);
      }

      AutofillForm(suggestion.type, suggestion.payload, metadata,
                   /*is_preview=*/false, GetTriggerSource());
      break;
    }
    case SuggestionType::kAddressFieldByFieldFilling:
      CHECK(suggestion.field_by_field_filling_type_used);
      if (std::optional<AutofillProfile> profile =
              GetProfileFromAddressSuggestion(suggestion)) {
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
      AutofillForm(suggestion.type, suggestion.payload, metadata,
                   /*is_preview=*/false, GetTriggerSource());
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
          metadata.row, FillingProduct::kCreditCard,
          manager_->client().IsOffTheRecord());
      AutofillForm(suggestion.type, suggestion.payload, metadata,
                   /*is_preview=*/false, GetTriggerSource());
      break;
    case SuggestionType::kVirtualCreditCardEntry:
      // There can be multiple virtual credit cards that all rely on
      // SuggestionType::kVirtualCreditCardEntry as a `type`.
      // In this case, the payload contains the backend id, which is a GUID
      // that identifies the actually chosen credit card.
      AutofillForm(suggestion.type, suggestion.payload, metadata,
                   /*is_preview=*/false, GetTriggerSource());
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
    case SuggestionType::kSaveAndFillCreditCardEntry: {
      payments::SaveAndFillManager* save_and_fill_manager =
          manager_->client()
              .GetPaymentsAutofillClient()
              ->GetSaveAndFillManager();
      CHECK(save_and_fill_manager);

      save_and_fill_manager->OnDidAcceptCreditCardSaveAndFillSuggestion(
          base::BindOnce(
              [](base::WeakPtr<AutofillExternalDelegate> delegate,
                 const CreditCard& card) {
                if (delegate) {
                  delegate->manager_->FillOrPreviewForm(
                      mojom::ActionPersistence::kFill, delegate->query_form_,
                      delegate->query_field_.global_id(), &card,
                      AutofillTriggerSource::kCreditCardSaveAndFill);
                }
              },
              GetWeakPtr()));

      manager_->GetCreditCardFormEventLogger()
          .OnDidAcceptSaveAndFillSuggestion();
      break;
    }
    case SuggestionType::kScanCreditCard:
      manager_->client().GetPaymentsAutofillClient()->ScanCreditCard(
          base::BindOnce(&AutofillExternalDelegate::OnCreditCardScanned,
                         GetWeakPtr()));
      break;
    case SuggestionType::kBnplEntry: {
      payments::BnplManager* bnpl_manager = manager_->GetPaymentsBnplManager();
      CHECK(bnpl_manager);

      bnpl_manager->OnDidAcceptBnplSuggestion(
          /*final_checkout_amount=*/suggestion
              .GetPayload<Suggestion::PaymentsPayload>()
              .extracted_amount_in_micros,
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
  if (std::ranges::contains(shown_suggestion_types_,
                            SuggestionType::kScanCreditCard)) {
    AutofillMetrics::LogScanCreditCardPromptMetric(
        suggestion.type == SuggestionType::kScanCreditCard
            ? AutofillMetrics::SCAN_CARD_ITEM_SELECTED
            : AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED);
  }
}

void AutofillExternalDelegate::MaybeAuthenticateBeforeFilling(
    const std::u16string& reauth_message,
    std::string histogram,
    base::OnceCallback<void(bool)> callback) {
  if (authenticator_) {
    authenticator_->Cancel();
    authenticator_.reset();
  }
  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator =
      manager_->client().GetDeviceAuthenticator(std::move(histogram));

  if (!authenticator ||
      !authenticator->CanAuthenticateWithBiometricOrScreenLock()) {
    std::move(callback).Run(/*auth_succeeded=*/true);
    return;
  }

  authenticator_ = std::move(authenticator);
  authenticator_->AuthenticateWithMessage(
      reauth_message,
      base::BindOnce(&AutofillExternalDelegate::OnReauthCompleted, GetWeakPtr(),
                     std::move(callback)));
}

void AutofillExternalDelegate::FillAutofillAiFormAndHidePopup(
    const Suggestion& suggestion) {
  const base::optional_ref<const EntityInstance> entity =
      GetEntityInstance(suggestion);
  auto [form_structure, autofill_field] = GetQueriedFormAndField();
  AutofillClient& client = manager_->client();
  if (!entity || !autofill_field) {
    client.HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion);
    return;
  }

  base::OnceCallback<void(std::optional<EntityInstance>)> fill_and_hide =
      base::BindOnce(
          [](base::WeakPtr<BrowserAutofillManager> manager,
             const FormData& form, const FieldGlobalId& field_id,
             AutofillTriggerSource trigger_source,
             std::optional<EntityInstance> entity) {
            if (!manager) {
              return;
            }
            if (entity) {
              manager->FillOrPreviewForm(mojom::ActionPersistence::kFill, form,
                                         field_id, &*entity, trigger_source);
            } else {
              manager->client().ShowAutofillAiFailureNotification(
                  l10n_util::GetStringUTF16(
                      IDS_AUTOFILL_AI_WALLET_FETCH_FAILURE_NOTIFICATION));
            }
          },
          manager_->GetBrowserAutofillManagerWeakPtr(), query_form_,
          query_field_.global_id(), GetTriggerSource())
          .Then(base::BindOnce(&AutofillClient::HideAutofillSuggestions,
                               client.GetWeakPtr(),
                               SuggestionHidingReason::kAcceptSuggestion));

  const bool is_sensitive = WillFillSensitiveAttributes(
      *entity, *form_structure, autofill_field->section(),
      client.GetAppLocale());
  const bool should_fetch_from_server =
      is_sensitive && entity->IsMaskedServerEntity() &&
      base::FeatureList::IsEnabled(features::kAutofillAiWalletPrivatePasses);
  if (should_fetch_from_server) {
    fill_and_hide = base::BindOnce(
        [](WalletPassAccessManager* wallet_pass_access_manager,
           base::OnceCallback<void(std::optional<EntityInstance>)> callback,
           std::optional<EntityInstance> masked_entity) {
          if (!masked_entity || !wallet_pass_access_manager) {
            // Close the popup.
            std::move(callback).Run(std::nullopt);
            return;
          }
          wallet_pass_access_manager->GetUnmaskedWalletEntityInstance(
              masked_entity->guid(), std::move(callback));
        },
        client.GetWalletPassAccessManager(), std::move(fill_and_hide));
  }

  const bool should_reauth =
      is_sensitive &&
      prefs::IsAutofillAiReauthBeforeFillingEnabled(client.GetPrefs());
  // Show a loading state during fetching or reauth.
  if ((should_fetch_from_server || should_reauth) &&
      base::FeatureList::IsEnabled(features::kAutofillAiWalletPrivatePasses)) {
    AttemptToDisplayAutofillSuggestions(
        PrepareLoadingStateSuggestions(
            base::ToVector(client.GetAutofillSuggestions()), suggestion),
        trigger_source_,
        /*is_update=*/true, AutofillSuggestionsIgnoreFocusLoss(true));
  }

  if (!should_reauth) {
    std::move(fill_and_hide).Run(*entity);
    return;
  }

  // Authenticate and fill on success.
  std::u16string message;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_IOS)
  const std::u16string origin =
      base::UTF8ToUTF16(autofill_field->origin().host());
  message = l10n_util::GetStringFUTF16(IDS_AUTOFILL_AI_FILLING_REAUTH, origin);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_IOS)
  base::OnceCallback<std::optional<EntityInstance>(bool)>
      convert_auth_response = base::BindOnce(
          [](EntityInstance masked_entity, bool auth_succeeded) {
            return auth_succeeded ? std::move(masked_entity)
                                  : std::optional<EntityInstance>();
          },
          *entity);
  MaybeAuthenticateBeforeFilling(
      message, "Autofill.Ai.ReauthToFill",
      std::move(convert_auth_response).Then(std::move(fill_and_hide)));
}

void AutofillExternalDelegate::OnReauthCompleted(
    base::OnceCallback<void(bool)> callback,
    bool auth_succeeded) {
  authenticator_.reset();
  std::move(callback).Run(auth_succeeded);
}

}  // namespace autofill
