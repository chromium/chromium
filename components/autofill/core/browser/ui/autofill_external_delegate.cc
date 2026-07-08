// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_external_delegate.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/types/zip.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/at_memory/at_memory_manager.h"
#include "components/autofill/core/browser/autofill_ai_form_rationalization.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/valuables/valuable_types.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/addresses/field_filling_address_util.h"
#include "components/autofill/core/browser/filling/autofill_ai/autofill_ai_access_manager.h"
#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/filling/form_filler.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#include "components/autofill/core/browser/integrators/compose/autofill_compose_delegate.h"
#include "components/autofill/core/browser/integrators/identity_credential/identity_credential_delegate.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_suggestion.h"
#include "components/autofill/core/browser/metrics/autofill_in_devtools_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/loyalty_cards_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/save_and_fill_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/browser/ui/tabbed_pane_enums.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

namespace {

// Fills the queried form with the provided credit card using the specified
// trigger source. Used as a callback for asynchronous card fetches.
void OnCreditCardFetched(base::WeakPtr<BrowserAutofillManager> manager,
                         AutofillTriggerSource trigger_source,
                         const FormGlobalId& form_id,
                         const FieldGlobalId& field_id,
                         const CreditCard& card) {
  if (manager) {
    manager->FillOrPreviewForm(mojom::ActionPersistence::kFill, form_id,
                               field_id, &card, trigger_source,
                               /*blocked_fields=*/{});
  }
}

// Fills the queried form with the provided `EntityInstance` in `result`,
// unless a `FailureReason` is present.
void OnEntityInstanceFetched(
    base::WeakPtr<BrowserAutofillManager> manager,
    AutofillTriggerSource trigger_source,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const FieldTypeSet& ai_field_types,
    base::expected<EntityInstance, AutofillAiAccessManager::FailureReason>
        result,
    bool reauth_attempted) {
  if (!manager) {
    return;
  }
  if (reauth_attempted) {
    const bool auth_succeeded =
        result.has_value() ||
        result.error() != AutofillAiAccessManager::FailureReason::kReauthFailed;
    LogReauthToFillResultPerFieldType(ai_field_types, auth_succeeded);
  }

  if (result.has_value()) {
    manager->FillOrPreviewForm(mojom::ActionPersistence::kFill, form_id,
                               field_id, &result.value(), trigger_source,
                               /*blocked_fields=*/{});
  } else if (result.error() ==
             AutofillAiAccessManager::FailureReason::kFetchFailed) {
    manager->client().ShowAutofillAiFetchFromWalletFailureNotification();
  }

  manager->client().HideSuggestions(SuggestionHidingReason::kAcceptSuggestion,
                                    FillingProduct::kAutofillAi);
}

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
bool HasAutofillSuggestionsForA11y(SuggestionType type) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillDoNotUpdateAutofillAvailabilityOnFocusEvents)) {
    switch (type) {
      // TODO(crbug.com/374918460): Consider adding other types that can be
      // classified as "providing autofill capabilities".
      case SuggestionType::kFillAutofillAi:
      case SuggestionType::kLoyaltyCardEntry:
        return true;
      default:
        return AutofillExternalDelegate::IsAutofillAndFirstLayerSuggestionId(
            type);
    }
  }

  switch (type) {
    case SuggestionType::kAddressEntry:
    case SuggestionType::kAddressEntryOnTyping:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kIdentityCredential:
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kOneTimePasswordEntry:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kVirtualCreditCardEntry:
      return true;
    case SuggestionType::kAutocompleteEntry:
    // Autocomplete entries are handled separately by the caller. The other
    // entries should not have announcements.
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kAtMemoryGenericError:
    case SuggestionType::kAtMemoryInactivityNudge:
    case SuggestionType::kAtMemoryNoConnection:
    case SuggestionType::kAtMemorySearchAffordance:
    case SuggestionType::kAtMemorySearchResult:
    case SuggestionType::kAutocompleteAtMemoryButton:
    case SuggestionType::kAutofillAiOtherOrders:
    case SuggestionType::kAutofillAiPrivateInferenceNotice:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kBnplFootnote:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kFetchingAmbientData:
    case SuggestionType::kFillPassword:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageAutofillAiIdentityDocs:
    case SuggestionType::kManageAutofillAiShopping:
    case SuggestionType::kManageAutofillAiTravel:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kMaximizeCreditCardBenefitsEntry:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kOpenGemini:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kPersonalContextNotice:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kSeparator:
    case SuggestionType::kTitle:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnPasskeyQrCode:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
      return false;
  }
}

}  // namespace

// Loads the AutofillProfile from the address data manager and returns a copy
// of it if exists or `std::nullopt` otherwise. In case the payload contains a
// non-empty email override, it applies it to the profile before returning it.
std::optional<AutofillProfile> GetProfileFromPayload(
    const AddressDataManager& adm,
    const Suggestion::AutofillProfilePayload& payload) {
  if (const AutofillProfile* profile =
          adm.GetProfileByGUID(payload.guid.value())) {
    return *profile;
  }
  return std::nullopt;
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
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kOneTimePasswordEntry:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    // Virtual cards can appear on their own when filling the CVC for a card
    // that a merchant has saved. This indicates there could be Autofill
    // suggestions related to standalone CVC fields.
    case SuggestionType::kVirtualCreditCardEntry:
      return true;
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kAtMemoryGenericError:
    case SuggestionType::kAtMemoryInactivityNudge:
    case SuggestionType::kAtMemoryNoConnection:
    case SuggestionType::kAtMemorySearchAffordance:
    case SuggestionType::kAtMemorySearchResult:
    case SuggestionType::kAutocompleteAtMemoryButton:
    case SuggestionType::kAutocompleteEntry:
    case SuggestionType::kAutofillAiOtherOrders:
    case SuggestionType::kAutofillAiPrivateInferenceNotice:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kBnplFootnote:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kFetchingAmbientData:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kFillPassword:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kIdentityCredential:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageAutofillAiIdentityDocs:
    case SuggestionType::kManageAutofillAiShopping:
    case SuggestionType::kManageAutofillAiTravel:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kMaximizeCreditCardBenefitsEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kOpenGemini:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kPersonalContextNotice:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kSeparator:
    case SuggestionType::kTitle:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnPasskeyQrCode:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
      return false;
  }
}

void AutofillExternalDelegate::OnQuery(
    const FormData& form,
    const FormFieldData& field,
    const gfx::Rect& caret_bounds,
    AutofillSuggestionTriggerSource trigger_source) {
  last_query_ = {.form_id = form.global_id(),
                 .field_id = field.global_id(),
                 .field_datalist_options = field.datalist_options()};
  caret_bounds_ = caret_bounds;
  trigger_source_ = trigger_source;
  manager_->client().UpdateAutofillDataListValues(field.datalist_options());
}

const AutofillField* AutofillExternalDelegate::GetQueriedField() const {
  return GetQueriedFormAndField().second;
}

std::pair<const FormStructure*, const AutofillField*>
AutofillExternalDelegate::GetQueriedFormAndField() const {
  const FormStructure* form_structure =
      manager_->FindCachedFormById(last_query_.form_id);
  if (!form_structure) {
    return {nullptr, nullptr};
  }
  return {form_structure, form_structure->GetFieldById(last_query_.field_id)};
}

AutofillTriggerSource AutofillExternalDelegate::GetTriggerSource() const {
  return TriggerSourceFromSuggestionTriggerSource(trigger_source_);
}

void AutofillExternalDelegate::OnSuggestionsReturned(
    const FormFieldData& trigger_field,
    const std::vector<Suggestion>& input_suggestions) {
  // These are guards against outdated suggestion results.
  if (trigger_field.global_id() != last_query_.field_id) {
    return;
  }
#if BUILDFLAG(IS_IOS)
  if (!manager_->client().IsLastQueriedField(trigger_field.global_id())) {
    return;
  }
#endif
  AttemptToDisplayAutofillSuggestions(
      input_suggestions, trigger_source_, trigger_field,
      AutofillSuggestionsIgnoreFocusLoss(false));
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
    base::optional_ref<const FormFieldData> trigger_field,
    AutofillSuggestionsIgnoreFocusLoss ignore_focus_loss) {
  const bool is_update = !trigger_field.has_value();
  CHECK(!*ignore_focus_loss || is_update)
      << "Ignoring focus loss is only supported for updates";

  if ((!is_update && !trigger_field->is_focusable()) ||
      !manager_->driver().CanShowAutofillUi()) {
    return;
  }

  PossiblyRemoveAutofillWarnings(suggestions);

  // If anything else is added to modify the values after inserting the data
  // list, AutofillPopupControllerImpl::UpdateDataListValues will need to be
  // updated to match.
  InsertDataListValues(last_query_.field_datalist_options, suggestions);

  // TODO(crbug.com/362630793): Try to eliminate this state. The controller
  // should be the one that knows about what suggestions were shown and passes
  // it on, not AED.
  trigger_source_ = trigger_source;

  shown_suggestion_types_ = base::ToVector(suggestions, &Suggestion::type);

  if (suggestions.empty() && !IsAtMemoryTriggerSource(trigger_source)) {
    OnAutofillAvailabilityEvent(
        mojom::AutofillSuggestionAvailability::kNoSuggestions);
    // No suggestions, any popup currently showing is obsolete.
    if (!manager_->client().IsAndroidLargeFormFactor() ||
        !base::FeatureList::IsEnabled(
            features::kAutofillAndroidKeyboardAccessoryDynamicPositioning)) {
      manager_->client().HideSuggestions(SuggestionHidingReason::kNoSuggestions,
                                         /*product=*/std::nullopt);
      return;
    }
  }

  // Send to display.
  if (is_update) {
    manager_->client().UpdateAutofillSuggestions(
        suggestions, GetMainFillingProduct(), trigger_source_,
        ignore_focus_loss);
    return;
  }

  CHECK(trigger_field);
  AutofillComposeDelegate* delegate = manager_->client().GetComposeDelegate();
  const bool show_proactive_nudge_at_caret =
      shown_suggestion_types_.size() == 1 &&
      shown_suggestion_types_[0] == SuggestionType::kComposeProactiveNudge &&
      (delegate && delegate->ShouldAnchorNudgeOnCaret());
  const bool are_caret_bounds_valid =
      caret_bounds_ != gfx::Rect() &&
      trigger_field->bounds().Contains(gfx::RectF(caret_bounds_));
  const bool is_at_memory = IsAtMemoryTriggerSource(trigger_source_);
  const bool should_use_caret_bounds =
      (show_proactive_nudge_at_caret || is_at_memory) && are_caret_bounds_valid;

  const PopupAnchorType default_anchor_type =
#if BUILDFLAG(IS_ANDROID)
      PopupAnchorType::kKeyboardAccessory;
#else
      PopupAnchorType::kField;
#endif

  const bool show_tabbed_popup = ShouldShowPayNowPayLaterTabs();
  if (show_tabbed_popup) {
    manager_->client()
        .GetPersonalDataManager()
        .payments_data_manager()
        .SetAutofillHasSeenBnpl();
  }

  // Tabbed popups may call `OnSuggestionsChanged()` when the active tab is
  // updated, so prefer the previous arrow side to keep the dropdown in
  // the same place relative to the field after the update.
  const bool prefer_prev_arrow_side_on_suggestions_update = show_tabbed_popup;

  PopupAnchorType anchor_type =
      should_use_caret_bounds ? PopupAnchorType::kCaret : default_anchor_type;
#if BUILDFLAG(IS_ANDROID)
  if (is_at_memory) {
    anchor_type = PopupAnchorType::kAtMemoryBottomSheet;
  }
#endif

  AutofillClient::PopupOpenArgs open_args(
      trigger_field->global_id().frame_token,
      should_use_caret_bounds ? gfx::RectF(caret_bounds_)
                              : trigger_field->bounds(),
      trigger_field->text_direction(), std::move(suggestions), trigger_source_,
      trigger_field->form_control_ax_id(), anchor_type, show_tabbed_popup,
      prefer_prev_arrow_side_on_suggestions_update);
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
            /*trigger_field=*/std::nullopt,
            AutofillSuggestionsIgnoreFocusLoss(false));
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
      last_query_.field_id, suggestion_availability);
}

std::variant<AutofillDriver*, password_manager::PasswordManagerDriver*>
AutofillExternalDelegate::GetDriver_DoNotUse() {
  return &manager_->driver();
}

void AutofillExternalDelegate::OnSuggestionsShown(
    base::span<const Suggestion> suggestions,
    base::optional_ref<const SuggestionMetadata> parent_suggestion_metadata) {
  // Popups are expected to be Autofill or Autocomplete.
  DCHECK(suggestions.empty() ||
         GetFillingProductFromSuggestionType(suggestions[0].type) !=
             FillingProduct::kPassword);

  const DenseSet<SuggestionType> shown_suggestion_types(suggestions,
                                                        &Suggestion::type);
  const bool is_subpopup = parent_suggestion_metadata.has_value();

  if (!is_subpopup) {
    if (std::ranges::any_of(shown_suggestion_types,
                            HasAutofillSuggestionsForA11y)) {
      OnAutofillAvailabilityEvent(
          mojom::AutofillSuggestionAvailability::kAutofillAvailable);
    } else if (shown_suggestion_types.contains(
                   SuggestionType::kAutocompleteEntry)) {
      OnAutofillAvailabilityEvent(
          mojom::AutofillSuggestionAvailability::kAutocompleteAvailable);
      if (autofill_metrics::ShouldLogAutofillSuggestionShown(trigger_source_)) {
        AutofillMetrics::OnAutocompleteSuggestionsShown();
      }
    }
  }

  manager_->DidShowSuggestions(
      suggestions, parent_suggestion_metadata, last_query_.form_id,
      last_query_.field_id, CreateUpdateSuggestionsCallback(), trigger_source_);
}

void AutofillExternalDelegate::OnSuggestionsHidden(
    SuggestionHidingReason reason) {
  manager_->GetAtMemoryManager().OnPopupHidden();
  manager_->OnSuggestionsHidden(reason);
}

bool AutofillExternalDelegate::OnFilterChanged(const std::u16string& filter) {
  return manager_->GetAtMemoryManager().OnFilterChanged(filter);
}

bool AutofillExternalDelegate::OnSearchSubmitted(const std::u16string& filter) {
  return manager_->GetAtMemoryManager().OnSearchSubmitted(filter);
}

bool AutofillExternalDelegate::IsSearching() const {
  return manager_->GetAtMemoryManager().IsSearching();
}

void AutofillExternalDelegate::DidSelectSuggestion(
    const Suggestion& suggestion) {
  ClearPreviewedForm();

  switch (suggestion.type) {
    case SuggestionType::kUndoOrClear:
      manager_->UndoAutofill(mojom::ActionPersistence::kPreview,
                             last_query_.form_id, last_query_.field_id);
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
                                   last_query_.form_id, last_query_.field_id,
                                   suggestion.main_text.value,
                                   FillingProduct::kAutocomplete,
                                   /*field_type_used=*/std::nullopt);
      break;
    case SuggestionType::kIbanEntry:
      // Always shows the masked IBAN value as the preview of the suggestion.
      manager_->FillOrPreviewField(mojom::ActionPersistence::kPreview,
                                   mojom::FieldActionType::kReplaceAll,
                                   last_query_.form_id, last_query_.field_id,
                                   suggestion.labels.empty()
                                       ? suggestion.main_text.value
                                       : suggestion.labels[0][0].value,
                                   FillingProduct::kIban, IBAN_VALUE);
      break;
    case SuggestionType::kMerchantPromoCodeEntry:
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kPreview,
          mojom::FieldActionType::kReplaceAll, last_query_.form_id,
          last_query_.field_id, suggestion.main_text.value,
          FillingProduct::kMerchantPromoCode, MERCHANT_PROMO_CODE);
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
                                    last_query_.form_id, last_query_.field_id,
                                    entity.as_ptr(), GetTriggerSource(),
                                    /*blocked_fields=*/{});
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
                                  last_query_.form_id, last_query_.field_id,
                                  &profile, GetTriggerSource(),
                                  /*blocked_fields=*/{});
      break;
    }
    case SuggestionType::kLoyaltyCardEntry:
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kPreview,
          mojom::FieldActionType::kReplaceAll, last_query_.form_id,
          last_query_.field_id, suggestion.main_text.value,
          FillingProduct::kLoyaltyCard, LOYALTY_MEMBERSHIP_ID);
      break;
    case SuggestionType::kAtMemorySearchResult:
      manager_->GetAtMemoryManager().FillOrPreviewSearchResult(
          mojom::ActionPersistence::kPreview, last_query_.form_id,
          last_query_.field_id, suggestion);
      break;
    case SuggestionType::kWebauthnPasskeyQrCode:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
      manager_->DelegateSelectToPasswordManager(suggestion,
                                                last_query_.field_id);
      break;
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kAtMemoryGenericError:
    case SuggestionType::kAtMemoryInactivityNudge:
    case SuggestionType::kAtMemoryNoConnection:
    case SuggestionType::kAtMemorySearchAffordance:
    case SuggestionType::kAutocompleteAtMemoryButton:
    case SuggestionType::kAutofillAiOtherOrders:
    case SuggestionType::kAutofillAiPrivateInferenceNotice:
    case SuggestionType::kBnplEntry:
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
    case SuggestionType::kManageAutofillAiShopping:
    case SuggestionType::kManageAutofillAiTravel:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kMaximizeCreditCardBenefitsEntry:
    case SuggestionType::kMixedFormMessage:
    // So far OTP suggestions are only available on Android, so no preview
    // is needed. This needs to be changed once Desktop suggestions and UI
    // are implemented.
    case SuggestionType::kOneTimePasswordEntry:
    case SuggestionType::kOpenGemini:
    case SuggestionType::kPersonalContextNotice:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeePromoCodeDetails:
      break;
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kBnplFootnote:
    case SuggestionType::kFetchingAmbientData:
    case SuggestionType::kFillPassword:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kSeparator:
    case SuggestionType::kTitle:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kWebauthnCredential:
      NOTREACHED();  // Should be handled elsewhere.
  }
}

void AutofillExternalDelegate::DidAcceptSuggestion(
    const Suggestion& suggestion,
    const SuggestionMetadata& metadata) {
  CHECK(suggestion.IsAcceptable());
  base::UmaHistogramEnumeration("Autofill.Suggestions.AcceptedType",
                                suggestion.type);

  const auto [form_structure, autofill_field] = GetQueriedFormAndField();
  if (form_structure && autofill_field) {
    manager_->client().GetFormInteractionsUkmLogger().LogSuggestionAccepted(
        manager_->driver().GetPageUkmSourceId(), CHECK_DEREF(form_structure),
        CHECK_DEREF(autofill_field), suggestion.type, metadata.row());
  }

  switch (suggestion.type) {
    case SuggestionType::kAddressEntry:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kDevtoolsTestAddressEntry:
      DidAcceptAddressSuggestion(suggestion, metadata);
      break;
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kVirtualCreditCardEntry:
      DidAcceptPaymentsSuggestion(suggestion, metadata);
      break;
    case SuggestionType::kMaximizeCreditCardBenefitsEntry:
      DidAcceptPaymentsSuggestion(suggestion, metadata);
      // For `kMaximizeCreditCardBenefitsEntry`, the popup remains open as it is
      // needed to display the best reward card from Gemini.
      return;
    case SuggestionType::kBnplEntry:
      DidAcceptPaymentsSuggestion(suggestion, metadata);
      if (base::FeatureList::IsEnabled(
              features::kAutofillEnablePayNowPayLaterTabs)) {
        // The popup will instead be closed by `BnplManager`.
        return;
      }
      break;
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageAutofillAiIdentityDocs:
    case SuggestionType::kManageAutofillAiShopping:
    case SuggestionType::kManageAutofillAiTravel:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard: {
      manager_->client().ShowAutofillSettings(suggestion.type);
      break;
    }
    case SuggestionType::kUndoOrClear:
      manager_->UndoAutofill(mojom::ActionPersistence::kFill,
                             last_query_.form_id, last_query_.field_id);
      break;
    case SuggestionType::kDatalistEntry:
      manager_->driver().RendererShouldAcceptDataListSuggestion(
          last_query_.field_id, suggestion.main_text.value);
      break;
    case SuggestionType::kAutocompleteEntry:
      AutofillMetrics::LogAutocompleteEvent(
          AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_SELECTED);
      autofill_metrics::LogSuggestionAcceptedIndex(
          metadata.row(), FillingProduct::kAutocomplete,
          manager_->client().IsOffTheRecord());
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          last_query_.form_id, last_query_.field_id, suggestion.main_text.value,
          FillingProduct::kAutocomplete,
          /*field_type_used=*/std::nullopt);
      manager_->OnSingleFieldSuggestionSelected(suggestion, last_query_.form_id,
                                                last_query_.field_id);
      break;
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
      if (AutofillComposeDelegate* delegate =
              manager_->client().GetComposeDelegate()) {
        delegate->OpenCompose(
            manager_->driver(), last_query_.field_id,
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
    case SuggestionType::kFillAutofillAi: {
      const base::optional_ref<const EntityInstance> entity =
          GetEntityInstance(suggestion);
      if (!entity || !autofill_field || !form_structure) {
        manager_->client().HideSuggestions(
            SuggestionHidingReason::kAcceptSuggestion,
            FillingProduct::kAutofillAi);
        return;
      }
      const bool will_fill_sensitive_info = WillFillSensitiveAttributes(
          *entity, *form_structure, autofill_field->section(),
          manager_->client().GetAppLocale());

      const bool is_async =
          manager_->GetAutofillAiAccessManager().FetchEntityInstance(
              *entity, will_fill_sensitive_info,
              base::BindOnce(&OnEntityInstanceFetched,
                             manager_->GetBrowserAutofillManagerWeakPtr(),
                             GetTriggerSource(), last_query_.form_id,
                             last_query_.field_id,
                             autofill_field->Type().GetAutofillAiTypes()));

      if (is_async &&
          (base::FeatureList::IsEnabled(features::kAutofillAmbientAutofill) ||
           base::FeatureList::IsEnabled(
               features::kAutofillAiWalletPrivatePasses))) {
        manager_->client().UpdateAutofillSuggestions(
            PrepareLoadingStateSuggestions(
                base::ToVector(manager_->client().GetAutofillSuggestions()),
                suggestion),
            FillingProduct::kAutofillAi, trigger_source_,
            AutofillSuggestionsIgnoreFocusLoss(true));
      }
      return;
    }
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
                [](base::WeakPtr<BrowserAutofillManager> manager,
                   const Suggestion& suggestion, const FormGlobalId& form_id,
                   const FieldGlobalId& field_id,
                   AutofillTriggerSource trigger_source, bool accepted) {
                  if (!manager || !accepted) {
                    return;
                  }

                  VerifiedProfile profile =
                      suggestion
                          .GetPayload<Suggestion::IdentityCredentialPayload>()
                          .fields;
                  manager->FillOrPreviewForm(mojom::ActionPersistence::kFill,
                                             form_id, field_id, &profile,
                                             trigger_source,
                                             /*blocked_fields=*/{});
                },
                manager_->GetBrowserAutofillManagerWeakPtr(), suggestion,
                last_query_.form_id, last_query_.field_id,
                TriggerSourceFromSuggestionTriggerSource(trigger_source_)));
      }
      break;
    }
    case SuggestionType::kLoyaltyCardEntry: {
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          last_query_.form_id, last_query_.field_id, suggestion.main_text.value,
          FillingProduct::kLoyaltyCard, LOYALTY_MEMBERSHIP_ID);
      const ValuablesDataManager& vdm =
          CHECK_DEREF(manager_->client().GetValuablesDataManager());
      const std::string guid =
          std::get<Suggestion::Guid>(suggestion.payload).value();
      if (std::optional<LoyaltyCard> loyalty_card =
              vdm.GetLoyaltyCardById(ValuableId(guid))) {
        manager_->LogAndRecordLoyaltyCardFill(
            *loyalty_card, last_query_.form_id, last_query_.field_id);
      }
      break;
    }
    case SuggestionType::kAllLoyaltyCardsEntry: {
      if (const auto& [form, field] = GetQueriedFormAndField(); form && field) {
        manager_->touch_to_fill_payment_method_delegate()
            ->ShowTouchToFillForAllLoyaltyCards(form->ToFormData(), *field);
      }
      break;
    }
    case SuggestionType::kOneTimePasswordEntry: {
      if (!form_structure || !autofill_field) {
        break;
      }
      OtpFillData otp_fill_data = CreateFillDataForOtpSuggestion(
          *form_structure, *autofill_field, suggestion.main_text.value);
      manager_->FillOrPreviewForm(mojom::ActionPersistence::kFill,
                                  last_query_.form_id, last_query_.field_id,
                                  &otp_fill_data, GetTriggerSource(),
                                  /*blocked_fields=*/{});
      break;
    }
    case SuggestionType::kAtMemoryInactivityNudge:
    case SuggestionType::kAutocompleteAtMemoryButton:
      manager_->driver().RendererShouldTriggerSuggestions(
          last_query_.field_id, AutofillSuggestionTriggerSource::kAtMemory);
      break;
    case SuggestionType::kAtMemorySearchResult:
      manager_->GetAtMemoryManager().FillOrPreviewSearchResult(
          mojom::ActionPersistence::kFill, last_query_.form_id,
          last_query_.field_id, suggestion, metadata);
      break;
    case SuggestionType::kOpenGemini:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      manager_->client().OpenGeminiInSidebar(
          suggestion.GetPayload<Suggestion::OpenGeminiPayload>().prompt);
      break;
#else
      NOTREACHED();
#endif
    case SuggestionType::kWebauthnPasskeyQrCode:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
      manager_->DelegateAcceptToPasswordManager(suggestion, metadata,
                                                last_query_.field_id);
      break;
    case SuggestionType::kAtMemorySearchAffordance:
      manager_->GetAtMemoryManager().OnSearchSubmitted(
          suggestion.main_text.value);
      // The popup remains open to show search results once the query completes.
      return;
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kAtMemoryGenericError:
    case SuggestionType::kAtMemoryNoConnection:
    case SuggestionType::kAutofillAiOtherOrders:
    case SuggestionType::kAutofillAiPrivateInferenceNotice:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kBnplFootnote:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kFetchingAmbientData:
    case SuggestionType::kFillPassword:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kPersonalContextNotice:
    case SuggestionType::kSeparator:
    case SuggestionType::kTitle:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kWebauthnCredential:
      NOTREACHED();  // Should be handled elsewhere.
  }

  manager_->client().HideSuggestions(SuggestionHidingReason::kAcceptSuggestion,
                                     /*product=*/std::nullopt);
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
    case SuggestionType::kAutocompleteEntry: {
      const AutocompleteEntry& entry =
          CHECK_DEREF(std::get_if<AutocompleteEntry>(&suggestion.payload));
      manager_->client()
          .GetSingleFieldFillRouter()
          .OnRemoveCurrentSingleFieldSuggestion(
              entry.key().name(), entry.key().value(), suggestion.type);
      return true;
    }
    // This suggestion type represents a notice about the usage of personal
    // context in autofill. The user can acknowledge it to dismiss it.
    case SuggestionType::kPersonalContextNotice: {
      manager_->client()
          .MarkPersonalContextAmbientAutofillNoticeAsAcknowledged();
      return true;
    }
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAddressEntryOnTyping:
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kAtMemoryGenericError:
    case SuggestionType::kAtMemoryInactivityNudge:
    case SuggestionType::kAtMemoryNoConnection:
    case SuggestionType::kAtMemorySearchAffordance:
    case SuggestionType::kAtMemorySearchResult:
    case SuggestionType::kAutocompleteAtMemoryButton:
    case SuggestionType::kAutofillAiOtherOrders:
    case SuggestionType::kAutofillAiPrivateInferenceNotice:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kBnplFootnote:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kFetchingAmbientData:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kFillPassword:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kIdentityCredential:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageAutofillAiIdentityDocs:
    case SuggestionType::kManageAutofillAiShopping:
    case SuggestionType::kManageAutofillAiTravel:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kMaximizeCreditCardBenefitsEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kOneTimePasswordEntry:
    case SuggestionType::kOpenGemini:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kSeparator:
    case SuggestionType::kTitle:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnPasskeyQrCode:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
      return false;
  }
}

void AutofillExternalDelegate::DidEndTextFieldEditing() {
  manager_->client().HideSuggestions(SuggestionHidingReason::kEndEditing,
                                     /*product=*/std::nullopt);
}

void AutofillExternalDelegate::OnTabSelected(TabbedPaneTabType tab_type) {
  switch (tab_type) {
    case TabbedPaneTabType::kPayLater:
      manager_->GetPaymentsBnplManager()->OnUserDecisionToUseBnpl(
          std::nullopt,
          base::BindOnce(
              [](base::WeakPtr<BrowserAutofillManager> manager,
                 const FormGlobalId& form_id, const FieldGlobalId& field_id,
                 const CreditCard& card) {
                if (manager) {
                  manager->FillOrPreviewForm(mojom::ActionPersistence::kFill,
                                             form_id, field_id, &card,
                                             AutofillTriggerSource::kPopup,
                                             /*blocked_fields=*/{});
                }
              },
              manager_->GetBrowserAutofillManagerWeakPtr(), last_query_.form_id,
              last_query_.field_id));
      break;
    case TabbedPaneTabType::kPayNow:
      manager_->GetPaymentsBnplManager()->OnUserDecisionToUseSavedCards();
      break;
  }
}

void AutofillExternalDelegate::ClearPreviewedForm() {
  manager_->driver().RendererShouldClearPreviewedForm();
}

FillingProduct AutofillExternalDelegate::GetMainFillingProduct() const {
  if (IsAtMemoryTriggerSource(trigger_source_)) {
    return FillingProduct::kAtMemory;
  }
  for (SuggestionType type : shown_suggestion_types_) {
    if (FillingProduct product = GetFillingProductFromSuggestionType(type);
        product != FillingProduct::kNone) {
      return product;
    }
  }
  return FillingProduct::kNone;
}

base::WeakPtr<AutofillExternalDelegate> AutofillExternalDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillExternalDelegate::PreviewAddressFieldByFieldFillingSuggestion(
    const AutofillProfile& profile,
    const Suggestion& suggestion) {
  const auto& [form, trigger_field] = GetQueriedFormAndField();
  if (!form || !trigger_field) {
    return;
  }
  const auto& [filling_value, select_text, filling_type] =
      GetFillingValueAndTypeForProfile(
          profile, manager_->client().GetAppLocale(),
          AutofillType(*suggestion.field_by_field_filling_type_used),
          *trigger_field, manager_->client().GetAddressNormalizer());
  if (!filling_value.empty()) {
    manager_->FillOrPreviewField(
        mojom::ActionPersistence::kPreview, mojom::FieldActionType::kReplaceAll,
        last_query_.form_id, last_query_.field_id, filling_value,
        FillingProduct::kAddress, suggestion.field_by_field_filling_type_used);
  }
}

void AutofillExternalDelegate::FillAddressFieldByFieldFillingSuggestion(
    const AutofillProfile& profile,
    const Suggestion& suggestion,
    const SuggestionMetadata& metadata) {
  const auto& [form, trigger_field] = GetQueriedFormAndField();
  if (!form || !trigger_field) {
    return;
  }
  const auto& [filling_value, select_text, filling_type] =
      GetFillingValueAndTypeForProfile(
          profile, manager_->client().GetAppLocale(),
          AutofillType(*suggestion.field_by_field_filling_type_used),
          *trigger_field, manager_->client().GetAddressNormalizer());
  if (!filling_value.empty()) {
    manager_->FillOrPreviewField(
        mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
        last_query_.form_id, last_query_.field_id, filling_value,
        FillingProduct::kAddress, suggestion.field_by_field_filling_type_used);
    if (suggestion.type == SuggestionType::kAddressFieldByFieldFilling) {
      // Ensure that `SuggestionType::kAddressEntryOnTyping` do not (at least
      // yet) affect key metrics.
      manager_->OnDidFillAddressFormFillingSuggestion(
          profile, last_query_.form_id, last_query_.field_id,
          GetTriggerSource());
    } else if (suggestion.type == SuggestionType::kAddressEntryOnTyping) {
      manager_->OnDidFillAddressOnTypingSuggestion(
          last_query_.field_id, filling_value,
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
      manager_->FillOrPreviewForm(action_persistence, last_query_.form_id,
                                  last_query_.field_id, &*profile,
                                  trigger_source,
                                  /*blocked_fields=*/{});
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
    manager_->FillOrPreviewForm(action_persistence, last_query_.form_id,
                                last_query_.field_id, &card_to_fill,
                                trigger_source,
                                /*blocked_fields=*/{});
  }
}

void AutofillExternalDelegate::InsertDataListValues(
    base::span<const SelectOption> datalist_options,
    std::vector<Suggestion>& suggestions) const {
  if (datalist_options.empty()) {
    return;
  }

  AutofillMetrics::LogDataListSuggestionsInserted();
  // Go through the list of autocomplete values and remove them if they are in
  // the list of datalist values.
  auto datalist_values = base::MakeFlatSet<std::u16string_view>(
      datalist_options, {},
      [](const SelectOption& option) -> std::u16string_view {
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
  suggestions.insert(suggestions.begin(), datalist_options.size(),
                     Suggestion(SuggestionType::kDatalistEntry));
  for (auto [suggestion, list_entry] :
       base::zip(suggestions, datalist_options)) {
    suggestion.main_text =
        Suggestion::Text(list_entry.value, Suggestion::Text::IsPrimary(true));
    suggestion.labels = {{Suggestion::Text(list_entry.text)}};
  }
}

void AutofillExternalDelegate::DidAcceptAddressSuggestion(
    const Suggestion& suggestion,
    const SuggestionMetadata& metadata) {
  const auto& [form, trigger_field] = GetQueriedFormAndField();
  if (!form || !trigger_field) {
    return;
  }
  base::UmaHistogramCounts100(
      "Autofill.Suggestion.AcceptanceFieldValueLength.Address",
      trigger_field->value().size());
  autofill_metrics::LogSuggestionAcceptedIndex(
      metadata.row(), FillingProduct::kAddress,
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
          form->form_signature(),
          CalculateFieldSignatureForField(*trigger_field), form->source_url());
#endif
}

void AutofillExternalDelegate::DidAcceptPaymentsSuggestion(
    const Suggestion& suggestion,
    const SuggestionMetadata& metadata) {
  const auto& [form, trigger_field] = GetQueriedFormAndField();
  if (!form || !trigger_field) {
    return;
  }
  base::UmaHistogramCounts100(
      "Autofill.Suggestion.AcceptanceFieldValueLength.CreditCard",
      trigger_field->value().size());
  switch (suggestion.type) {
    case SuggestionType::kCreditCardEntry:
      autofill_metrics::LogSuggestionAcceptedIndex(
          metadata.row(), FillingProduct::kCreditCard,
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
      // the IBAN value will be filled if the request is successful.
      manager_->client()
          .GetPaymentsAutofillClient()
          ->GetIbanAccessManager()
          ->FetchValue(
              suggestion.payload,
              base::BindOnce(
                  [](base::WeakPtr<BrowserAutofillManager> manager,
                     const FormGlobalId& form_id, const FieldGlobalId& field_id,
                     const std::u16string& value) {
                    if (manager) {
                      manager->FillOrPreviewField(
                          mojom::ActionPersistence::kFill,
                          mojom::FieldActionType::kReplaceAll, form_id,
                          field_id, value, FillingProduct::kIban, IBAN_VALUE);
                    }
                  },
                  manager_->GetBrowserAutofillManagerWeakPtr(),
                  last_query_.form_id, last_query_.field_id));
      manager_->OnSingleFieldSuggestionSelected(suggestion, last_query_.form_id,
                                                last_query_.field_id);
      break;
    case SuggestionType::kMerchantPromoCodeEntry:
      // User selected an Autocomplete or Merchant Promo Code field, so we fill
      // directly.
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          last_query_.form_id, last_query_.field_id, suggestion.main_text.value,
          FillingProduct::kMerchantPromoCode, MERCHANT_PROMO_CODE);
      manager_->OnSingleFieldSuggestionSelected(suggestion, last_query_.form_id,
                                                last_query_.field_id);
      break;
    case SuggestionType::kSeePromoCodeDetails:
      // Open a new tab and navigate to the offer details page.
      manager_->client()
          .GetPaymentsAutofillClient()
          ->OpenPromoCodeOfferDetailsURL(suggestion.GetPayload<GURL>());
      manager_->OnSingleFieldSuggestionSelected(suggestion, last_query_.form_id,
                                                last_query_.field_id);
      break;
    case SuggestionType::kSaveAndFillCreditCardEntry: {
      payments::SaveAndFillManager* save_and_fill_manager =
          manager_->client()
              .GetPaymentsAutofillClient()
              ->GetSaveAndFillManager();
      CHECK(save_and_fill_manager);

      save_and_fill_manager->OnDidAcceptCreditCardSaveAndFillSuggestion(
          base::BindOnce(&OnCreditCardFetched,
                         manager_->GetBrowserAutofillManagerWeakPtr(),
                         AutofillTriggerSource::kCreditCardSaveAndFill,
                         last_query_.form_id, last_query_.field_id));

      manager_->GetCreditCardFormEventLogger()
          .OnDidAcceptSaveAndFillSuggestion();
      break;
    }
    case SuggestionType::kScanCreditCard:
      manager_->client().GetPaymentsAutofillClient()->ScanCreditCard(
          base::BindOnce(&OnCreditCardFetched,
                         manager_->GetBrowserAutofillManagerWeakPtr(),
                         AutofillTriggerSource::kScanCreditCard,
                         last_query_.form_id, last_query_.field_id));
      break;
    case SuggestionType::kBnplEntry: {
      payments::BnplManager* bnpl_manager = manager_->GetPaymentsBnplManager();
      CHECK(bnpl_manager);

      if (base::FeatureList::IsEnabled(
              features::kAutofillEnablePayNowPayLaterTabs)) {
        bnpl_manager->OnIssuerAccepted(
            /*issuer=*/suggestion.GetPayload<Suggestion::BnplIssuer>().value());
      } else {
        bnpl_manager->OnUserDecisionToUseBnpl(
            /*final_checkout_amount=*/suggestion
                .GetPayload<Suggestion::PaymentsPayload>()
                .extracted_amount_in_micros,
            base::BindOnce(&OnCreditCardFetched,
                           manager_->GetBrowserAutofillManagerWeakPtr(),
                           AutofillTriggerSource::kPopup, last_query_.form_id,
                           last_query_.field_id));
      }
      break;
    }
    case SuggestionType::kMaximizeCreditCardBenefitsEntry: {
      // TODO(haochenf) Handle MaximizeCreditCardBenefitsEntry selection.
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

bool AutofillExternalDelegate::ShouldShowPayNowPayLaterTabs() {
  if (GetQueriedField()) {
    return GetMainFillingProduct() == FillingProduct::kCreditCard &&
           payments::ShouldShowBnplSuggestions(
               manager_->client(),
               GetQueriedField()->Type().GetCreditCardType()) &&
           base::FeatureList::IsEnabled(
               features::kAutofillEnablePayNowPayLaterTabs);
  }
  return false;
}

}  // namespace autofill
