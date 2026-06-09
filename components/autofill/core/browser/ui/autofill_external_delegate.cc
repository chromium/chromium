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
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
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
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kAddressEntryOnTyping:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kIdentityCredential:
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kOneTimePasswordEntry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kFillAutofillAi:
      return true;
    case SuggestionType::kAutocompleteEntry:
    // Autocomplete entries are handled separately by the caller. The other
    // entries should not have announcements.
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageAutofillAiIdentityDocs:
    case SuggestionType::kManageAutofillAiTravel:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kWebauthnPasskeyQrCode:
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kAtMemorySearchResult:
    case SuggestionType::kBnplFootnote:
    case SuggestionType::kAtMemoryInactivityNudge:
    case SuggestionType::kAutocompleteAtMemoryButton:
    case SuggestionType::kOpenGemini:
    case SuggestionType::kAtMemoryNoConnection:
    case SuggestionType::kAtMemorySearchAffordance:
    case SuggestionType::kPersonalContextNotice:
      return false;
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
    case SuggestionType::kWebauthnPasskeyQrCode:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kAtMemorySearchResult:
    case SuggestionType::kAtMemoryInactivityNudge:
    case SuggestionType::kBnplFootnote:
    case SuggestionType::kAutocompleteAtMemoryButton:
    case SuggestionType::kOpenGemini:
    case SuggestionType::kAtMemoryNoConnection:
    case SuggestionType::kAtMemorySearchAffordance:
    case SuggestionType::kPersonalContextNotice:
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

  if (!query_field_.is_focusable() || !manager_->driver().CanShowAutofillUi()) {
    return;
  }

  PossiblyRemoveAutofillWarnings(suggestions);
  // If anything else is added to modify the values after inserting the data
  // list, AutofillPopupControllerImpl::UpdateDataListValues will need to be
  // updated to match.
  InsertDataListValues(suggestions);

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

  AutofillClient::PopupOpenArgs open_args(
      should_use_caret_bounds ? gfx::RectF(caret_bounds_)
                              : query_field_.bounds(),
      query_field_.text_direction(), std::move(suggestions), trigger_source_,
      query_field_.form_control_ax_id(),
      should_use_caret_bounds ? PopupAnchorType::kCaret : default_anchor_type,
      show_tabbed_popup, prefer_prev_arrow_side_on_suggestions_update);
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

  manager_->DidShowSuggestions(
      suggestions, query_form_.global_id(), query_field_.global_id(),
      CreateUpdateSuggestionsCallback(), trigger_source_);
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
                             query_form_.global_id(), query_field_.global_id());
      break;
    case SuggestionType::kAddressEntry:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kDevtoolsTestAddressEntry:
      AutofillForm(suggestion.type, suggestion.payload,
                   /*metadata=*/std::nullopt,
                   /*is_preview=*/true, GetTriggerSource());
      break;
    case SuggestionType::kAutocompleteEntry:
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kPreview,
          mojom::FieldActionType::kReplaceAll, query_form_.global_id(),
          query_field_.global_id(), suggestion.main_text.value,
          FillingProduct::kAutocomplete,
          /*field_type_used=*/std::nullopt);
      break;
    case SuggestionType::kIbanEntry:
      // Always shows the masked IBAN value as the preview of the suggestion.
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kPreview,
          mojom::FieldActionType::kReplaceAll, query_form_.global_id(),
          query_field_.global_id(),
          suggestion.labels.empty() ? suggestion.main_text.value
                                    : suggestion.labels[0][0].value,
          FillingProduct::kIban, IBAN_VALUE);
      break;
    case SuggestionType::kMerchantPromoCodeEntry:
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kPreview,
          mojom::FieldActionType::kReplaceAll, query_form_.global_id(),
          query_field_.global_id(), suggestion.main_text.value,
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
        manager_->FillOrPreviewForm(
            mojom::ActionPersistence::kPreview, query_form_.global_id(),
            query_field_.global_id(), entity.as_ptr(), GetTriggerSource(),
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
      manager_->FillOrPreviewForm(
          mojom::ActionPersistence::kPreview, query_form_.global_id(),
          query_field_.global_id(), &profile, GetTriggerSource(),
          /*blocked_fields=*/{});
      break;
    }
    case SuggestionType::kLoyaltyCardEntry:
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kPreview,
          mojom::FieldActionType::kReplaceAll, query_form_.global_id(),
          query_field_.global_id(), suggestion.main_text.value,
          FillingProduct::kLoyaltyCard, LOYALTY_MEMBERSHIP_ID);
      break;
    case SuggestionType::kAtMemorySearchResult:
      manager_->GetAtMemoryManager().FillOrPreviewSearchResult(
          mojom::ActionPersistence::kPreview, query_form_.global_id(),
          query_field_.global_id(), suggestion);
      break;
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kWebauthnPasskeyQrCode:
      manager_->DelegateSelectToPasswordManager(suggestion, query_field_);
      break;
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kAtMemoryInactivityNudge:
    case SuggestionType::kAutocompleteAtMemoryButton:
    case SuggestionType::kOpenGemini:
    case SuggestionType::kAtMemoryNoConnection:
    case SuggestionType::kAtMemorySearchAffordance:
    case SuggestionType::kPersonalContextNotice:
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

  const auto [form_structure, autofill_field] = GetQueriedFormAndField();
  if (form_structure && autofill_field) {
    manager_->client().GetFormInteractionsUkmLogger().LogSuggestionAccepted(
        manager_->driver().GetPageUkmSourceId(), CHECK_DEREF(form_structure),
        CHECK_DEREF(autofill_field), suggestion.type, metadata.row);
  }

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
    case SuggestionType::kManageLoyaltyCard: {
      manager_->client().ShowAutofillSettings(suggestion.type);
      break;
    }
    case SuggestionType::kUndoOrClear:
      manager_->UndoAutofill(mojom::ActionPersistence::kFill,
                             query_form_.global_id(), query_field_.global_id());
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
          query_form_.global_id(), query_field_.global_id(),
          suggestion.main_text.value, FillingProduct::kAutocomplete,
          /*field_type_used=*/std::nullopt);
      manager_->OnSingleFieldSuggestionSelected(
          suggestion, query_form_.global_id(), query_field_.global_id());
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
              base::BindOnce(&AutofillExternalDelegate::OnEntityInstanceFetched,
                             GetWeakPtr(), GetTriggerSource(),
                             autofill_field->Type().GetAutofillAiTypes()));

      if (is_async && base::FeatureList::IsEnabled(
                          features::kAutofillAiWalletPrivatePasses)) {
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
                      mojom::ActionPersistence::kFill,
                      delegate->query_form_.global_id(),
                      delegate->query_field_.global_id(), &profile,
                      TriggerSourceFromSuggestionTriggerSource(
                          delegate->trigger_source_),
                      /*blocked_fields=*/{});
                },
                GetWeakPtr(), suggestion));
      }
      break;
    }
    case SuggestionType::kLoyaltyCardEntry: {
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          query_form_.global_id(), query_field_.global_id(),
          suggestion.main_text.value, FillingProduct::kLoyaltyCard,
          LOYALTY_MEMBERSHIP_ID);
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
      if (!form_structure || !autofill_field) {
        break;
      }
      OtpFillData otp_fill_data = CreateFillDataForOtpSuggestion(
          *form_structure, *autofill_field, suggestion.main_text.value);
      manager_->FillOrPreviewForm(mojom::ActionPersistence::kFill,
                                  query_form_.global_id(),
                                  query_field_.global_id(), &otp_fill_data,
                                  GetTriggerSource(), /*blocked_fields=*/{});
      break;
    }
    case SuggestionType::kAtMemoryInactivityNudge:
    case SuggestionType::kAutocompleteAtMemoryButton:
      manager_->driver().RendererShouldTriggerSuggestions(
          query_field_.global_id(), AutofillSuggestionTriggerSource::kAtMemory);
      break;
    case SuggestionType::kAtMemorySearchResult:
      manager_->GetAtMemoryManager().FillOrPreviewSearchResult(
          mojom::ActionPersistence::kFill, query_form_.global_id(),
          query_field_.global_id(), suggestion);
      break;
    case SuggestionType::kOpenGemini:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      manager_->client().OpenGeminiInSidebar(
          suggestion.GetPayload<Suggestion::OpenGeminiPayload>().prompt);
      break;
#else
      NOTREACHED();
#endif
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kWebauthnPasskeyQrCode:
      manager_->DelegateAcceptToPasswordManager(suggestion, metadata,
                                                query_field_);
      break;
    case SuggestionType::kAtMemorySearchAffordance:
      manager_->GetAtMemoryManager().OnSearchSubmitted(
          suggestion.main_text.value);
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
    case SuggestionType::kAtMemoryNoConnection:
    case SuggestionType::kPersonalContextNotice:
      NOTREACHED();  // Should be handled elsewhere.
  }

  if (suggestion.type == SuggestionType::kAtMemorySearchAffordance ||
      (suggestion.type == SuggestionType::kBnplEntry &&
       base::FeatureList::IsEnabled(
           features::kAutofillEnablePayNowPayLaterTabs))) {
    // Return early to prevent the popup from hiding.
    // For `kBnplEntry`, the popup will instead be closed by `BnplManager`.
    // For `kAtMemorySearchAffordance`, the popup remains open to show search
    // results once the query completes.
    return;
  }

  manager_->client().HideSuggestions(SuggestionHidingReason::kAcceptSuggestion,
                                     /*product=*/std::nullopt);
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
    case SuggestionType::kWebauthnPasskeyQrCode:
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
    case SuggestionType::kAtMemoryInactivityNudge:
    case SuggestionType::kBnplFootnote:
    case SuggestionType::kAutocompleteAtMemoryButton:
    case SuggestionType::kOpenGemini:
    case SuggestionType::kAtMemoryNoConnection:
    case SuggestionType::kAtMemorySearchAffordance:
    case SuggestionType::kPersonalContextNotice:
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
          std::nullopt, base::BindOnce(
                            [](base::WeakPtr<AutofillExternalDelegate> delegate,
                               const CreditCard& card) {
                              if (delegate) {
                                delegate->manager_->FillOrPreviewForm(
                                    mojom::ActionPersistence::kFill,
                                    delegate->query_form_.global_id(),
                                    delegate->query_field_.global_id(), &card,
                                    AutofillTriggerSource::kPopup,
                                    /*blocked_fields=*/{});
                              }
                            },
                            GetWeakPtr()));
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

void AutofillExternalDelegate::OnCreditCardFetched(
    AutofillTriggerSource trigger_source,
    const CreditCard& card) {
  manager_->FillOrPreviewForm(mojom::ActionPersistence::kFill,
                              query_form_.global_id(), query_field_.global_id(),
                              &card, trigger_source,
                              /*blocked_fields=*/{});
}

void AutofillExternalDelegate::OnEntityInstanceFetched(
    AutofillTriggerSource trigger_source,
    const FieldTypeSet& ai_field_types,
    base::expected<EntityInstance, AutofillAiAccessManager::FailureReason>
        result,
    bool reauth_attempted) {
  if (reauth_attempted) {
    const bool auth_succeeded =
        result.has_value() ||
        result.error() != AutofillAiAccessManager::FailureReason::kReauthFailed;
    LogReauthToFillResultPerFieldType(ai_field_types, auth_succeeded);
  }

  if (result.has_value()) {
    manager_->FillOrPreviewForm(
        mojom::ActionPersistence::kFill, query_form_.global_id(),
        query_field_.global_id(), &result.value(), trigger_source,
        /*blocked_fields=*/{});
  } else if (result.error() ==
             AutofillAiAccessManager::FailureReason::kFetchFailed) {
    manager_->client().ShowAutofillAiFetchFromWalletFailureNotification();
  }

  manager_->client().HideSuggestions(SuggestionHidingReason::kAcceptSuggestion,
                                     FillingProduct::kAutofillAi);
}

void AutofillExternalDelegate::PreviewAddressFieldByFieldFillingSuggestion(
    const AutofillProfile& profile,
    const Suggestion& suggestion) {
  const auto& [filling_value, select_text, filling_type] =
      GetFillingValueAndTypeForProfile(
          profile, manager_->client().GetAppLocale(),
          AutofillType(*suggestion.field_by_field_filling_type_used),
          query_field_, manager_->client().GetAddressNormalizer());
  if (!filling_value.empty()) {
    manager_->FillOrPreviewField(
        mojom::ActionPersistence::kPreview, mojom::FieldActionType::kReplaceAll,
        query_form_.global_id(), query_field_.global_id(), filling_value,
        FillingProduct::kAddress, suggestion.field_by_field_filling_type_used);
  }
}

void AutofillExternalDelegate::FillAddressFieldByFieldFillingSuggestion(
    const AutofillProfile& profile,
    const Suggestion& suggestion,
    const SuggestionMetadata& metadata) {
  const auto& [filling_value, select_text, filling_type] =
      GetFillingValueAndTypeForProfile(
          profile, manager_->client().GetAppLocale(),
          AutofillType(*suggestion.field_by_field_filling_type_used),
          query_field_, manager_->client().GetAddressNormalizer());
  if (!filling_value.empty()) {
    manager_->FillOrPreviewField(
        mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
        query_form_.global_id(), query_field_.global_id(), filling_value,
        FillingProduct::kAddress, suggestion.field_by_field_filling_type_used);
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
      manager_->FillOrPreviewForm(action_persistence, query_form_.global_id(),
                                  query_field_.global_id(), &*profile,
                                  trigger_source, /*blocked_fields=*/{});
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
    manager_->FillOrPreviewForm(action_persistence, query_form_.global_id(),
                                query_field_.global_id(), &card_to_fill,
                                trigger_source, /*blocked_fields=*/{});
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
                                   delegate->query_form_.global_id(),
                                   delegate->query_field_.global_id(), value,
                                   FillingProduct::kIban, IBAN_VALUE);
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
          query_form_.global_id(), query_field_.global_id(),
          suggestion.main_text.value, FillingProduct::kMerchantPromoCode,
          MERCHANT_PROMO_CODE);
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
          base::BindOnce(&AutofillExternalDelegate::OnCreditCardFetched,
                         GetWeakPtr(),
                         AutofillTriggerSource::kCreditCardSaveAndFill));

      manager_->GetCreditCardFormEventLogger()
          .OnDidAcceptSaveAndFillSuggestion();
      break;
    }
    case SuggestionType::kScanCreditCard:
      manager_->client().GetPaymentsAutofillClient()->ScanCreditCard(
          base::BindOnce(&AutofillExternalDelegate::OnCreditCardFetched,
                         GetWeakPtr(), AutofillTriggerSource::kScanCreditCard));
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
            base::BindOnce(&AutofillExternalDelegate::OnCreditCardFetched,
                           GetWeakPtr(), AutofillTriggerSource::kPopup));
      }
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
