// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_controller_impl.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check_op.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_view.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_utils.h"
#include "chrome/browser/ui/autofill/next_idle_barrier.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

using FillingSource = ManualFillingController::FillingSource;
using RemovalConfirmationText =
    AutofillKeyboardAccessoryController::RemovalConfirmationText;

constexpr std::u16string_view kLabelSeparator = u" ";
constexpr size_t kMaxBulletCount = 8;

constexpr std::u16string_view kHomeAddressManagementUrl =
    u"https://myaccount.google.com/address/"
    u"home?utm_source=chrome&utm_campaign=manage_addresses";
constexpr std::u16string_view kWorkAddressManagementUrl =
    u"https://myaccount.google.com/address/"
    u"work?utm_source=chrome&utm_campaign=manage_addresses";
constexpr std::u16string_view kAccountNameAndEmailManagementUrl =
    u"https://myaccount.google.com/personal-info"
    u"?utm_source=chrome-settings&utm_medium=autofill";

std::u16string ExtractPassword(const std::u16string& label) {
  // `label` is never empty since `Suggestion::labels` must contain a password.
  return label.substr(0, kMaxBulletCount);
}

Suggestion::Text FormatLabelsByFillingProduct(
    const std::u16string& label,
    const std::u16string& additional_label,
    FillingProduct filling_product) {
  switch (filling_product) {
    case FillingProduct::kPassword:
      // The `Suggestion::additional_label` contains the signon_realm or is
      // empty.
      return Suggestion::Text(
          additional_label.empty()
              ? ExtractPassword(label)
              : base::StrCat({additional_label, kLabelSeparator,
                              ExtractPassword(label)}));
    case FillingProduct::kAddress:
    case FillingProduct::kPlusAddresses:
    case FillingProduct::kCreditCard:
    case FillingProduct::kIban:
    case FillingProduct::kAutocomplete:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kCompose:
    case FillingProduct::kAutofillAi:
    case FillingProduct::kLoyaltyCard:
    case FillingProduct::kIdentityCredential:
    case FillingProduct::kDataList:
    case FillingProduct::kOneTimePassword:
    case FillingProduct::kPasskey:
    case FillingProduct::kNone:
      return Suggestion::Text(label);
  }
}

// Creates a text label used by the keyboard accessory. For password
// suggestions, constructs the label from the password stored in
// `Suggestion::additional_label` and an optional signon realm stored in
// `Suggestion::labels`. For other suggestions, constructs the label from
// `Suggestion::labels`.
Suggestion::Text CreateLabel(const Suggestion& suggestion) {
  if (suggestion.labels.empty()) {
    return Suggestion::Text();
  }
  // TODO(crbug.com/40221039): Re-consider whether using CHECK is an appropriate
  // way to explicitly regulate what information should be populated for the
  // interface.
  CHECK_EQ(suggestion.labels.size(), 1U);
  CHECK_EQ(suggestion.labels[0].size(), 1U);
  return FormatLabelsByFillingProduct(
      suggestion.labels[0][0].value, suggestion.additional_label,
      GetFillingProductFromSuggestionType(suggestion.type));
}

std::u16string GetAccountEmail(content::WebContents* web_contents) {
  if (!web_contents) {
    return {};
  }
  const std::optional<AccountInfo> account =
      GetPrimaryAccountInfoFromBrowserContext(
          web_contents->GetBrowserContext());
  return account ? base::UTF8ToUTF16(account->email) : std::u16string();
}

// Gets the text for a dialog to confirm removing an autocomplete suggestion.
// Returns `true` if the atucomplete entry can be deleted, `false` otherwise.
[[nodiscard]] bool GetAutocompleteRemovalText(
    const std::u16string& value,
    RemovalConfirmationText* removal_text) {
  if (removal_text) {
    removal_text->title = value;
    removal_text->body = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_CONFIRMATION_BODY);
    removal_text->confirm_button_text =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DELETE_SUGGESTION_BUTTON);
  }
  return true;
}

// Gets the text for a dialog to confirm removing a credit card suggestion.
// Returns `true` if the card can be deleted, `false` otherwise.
[[nodiscard]] bool GetCreditCardRemovalText(
    const Suggestion::Payload& payload,
    content::WebContents* web_contents,
    RemovalConfirmationText* removal_text) {
  if (!std::holds_alternative<Suggestion::Guid>(payload)) {
    return false;
  }
  PersonalDataManager* pdm = PersonalDataManagerFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());
  const CreditCard* credit_card =
      pdm->payments_data_manager().GetCreditCardByGUID(
          std::get<Suggestion::Guid>(payload).value());
  if (!credit_card || !CreditCard::IsLocalCard(credit_card)) {
    return false;
  }

  if (removal_text) {
    removal_text->title = credit_card->CardNameAndLastFourDigits();
    removal_text->body = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DELETE_CREDIT_CARD_SUGGESTION_CONFIRMATION_BODY);
    removal_text->confirm_button_text =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DELETE_SUGGESTION_BUTTON);
  }
  return true;
}

// Gets the text for a dialog to confirm removing an address suggestion.
// The text varies based on the profile type (e.g., local vs. Home/Work).
// Returns `true` if the address profile can be deleted, `false` otherwise.
[[nodiscard]] bool GetAddressRemovalText(
    const Suggestion::Payload& payload,
    const std::u16string& value,
    content::WebContents* web_contents,
    RemovalConfirmationText* removal_text) {
  if (!std::holds_alternative<Suggestion::AutofillProfilePayload>(payload)) {
    return false;
  }
  PersonalDataManager* pdm = PersonalDataManagerFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());
  const AutofillProfile* profile = pdm->address_data_manager().GetProfileByGUID(
      std::get<Suggestion::AutofillProfilePayload>(payload).guid.value());
  if (!profile) {
    return false;
  }

  if (removal_text) {
    switch (profile->record_type()) {
      case AutofillProfile::RecordType::kLocalOrSyncable:
      case AutofillProfile::RecordType::kAccount:
        if (std::u16string street_address =
                profile->GetRawInfo(ADDRESS_HOME_CITY);
            !street_address.empty()) {
          removal_text->title = std::move(street_address);
        } else {
          removal_text->title = value;
        }
        removal_text->body = l10n_util::GetStringUTF16(
            IDS_AUTOFILL_DELETE_PROFILE_SUGGESTION_CONFIRMATION_BODY);
        removal_text->confirm_button_text =
            l10n_util::GetStringUTF16(IDS_AUTOFILL_DELETE_SUGGESTION_BUTTON);
        break;
      case AutofillProfile::RecordType::kAccountHome:
        removal_text->title = l10n_util::GetStringUTF16(
            IDS_AUTOFILL_REMOVE_HOME_PROFILE_SUGGESTION_CONFIRMATION_TITLE);
        removal_text->body = l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_REMOVE_HOME_PROFILE_SUGGESTION_CONFIRMATION_BODY,
            GetAccountEmail(web_contents));
        removal_text->body_link = kHomeAddressManagementUrl;
        removal_text->confirm_button_text =
            l10n_util::GetStringUTF16(IDS_AUTOFILL_REMOVE_SUGGESTION_BUTTON);
        break;
      case AutofillProfile::RecordType::kAccountWork:
        removal_text->title = l10n_util::GetStringUTF16(
            IDS_AUTOFILL_REMOVE_WORK_PROFILE_SUGGESTION_CONFIRMATION_TITLE);
        removal_text->body = l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_REMOVE_WORK_PROFILE_SUGGESTION_CONFIRMATION_BODY,
            GetAccountEmail(web_contents));
        removal_text->body_link = kWorkAddressManagementUrl;
        removal_text->confirm_button_text =
            l10n_util::GetStringUTF16(IDS_AUTOFILL_REMOVE_SUGGESTION_BUTTON);
        break;
      case AutofillProfile::RecordType::kAccountNameEmail:
        removal_text->title = l10n_util::GetStringUTF16(
            IDS_AUTOFILL_REMOVE_ACCOUNT_NAME_AND_EMAIL_PROFILE_SUGGESTION_CONFIRMATION_TITLE);
        removal_text->body = l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_REMOVE_ACCOUNT_NAME_AND_EMAIL_PROFILE_SUGGESTION_CONFIRMATION_BODY,
            GetAccountEmail(web_contents));
        removal_text->body_link = kAccountNameAndEmailManagementUrl;
        removal_text->confirm_button_text =
            l10n_util::GetStringUTF16(IDS_AUTOFILL_REMOVE_SUGGESTION_BUTTON);
    }
  }
  return true;
}

}  // namespace

// static
base::WeakPtr<AutofillSuggestionController>
AutofillSuggestionController::GetOrCreate(
    base::WeakPtr<AutofillSuggestionController> previous,
    base::WeakPtr<AutofillSuggestionDelegate> delegate,
    content::WebContents* web_contents,
    PopupControllerCommon controller_common,
    int32_t form_control_ax_id) {
  // All controllers on Android derive from
  // `AutofillKeyboardAccessoryControllerImpl`.
  if (AutofillKeyboardAccessoryControllerImpl* previous_impl =
          static_cast<AutofillKeyboardAccessoryControllerImpl*>(previous.get());
      previous_impl && previous_impl->delegate_.get() == delegate.get() &&
      previous_impl->container_view() == controller_common.container_view) {
    if (previous_impl->self_deletion_weak_ptr_factory_.HasWeakPtrs()) {
      previous_impl->self_deletion_weak_ptr_factory_.InvalidateWeakPtrs();
    }
    previous_impl->controller_common_ = std::move(controller_common);
    previous_impl->suggestions_.clear();
    return previous_impl->GetWeakPtr();
  }

  if (previous) {
    previous->Hide(SuggestionHidingReason::kViewDestroyed);
  }
  auto* controller = new AutofillKeyboardAccessoryControllerImpl(
      delegate, web_contents, std::move(controller_common));
  return controller->GetWeakPtr();
}

AutofillKeyboardAccessoryControllerImpl::
    AutofillKeyboardAccessoryControllerImpl(
        base::WeakPtr<AutofillSuggestionDelegate> delegate,
        content::WebContents* web_contents,
        PopupControllerCommon controller_common)
    : delegate_(delegate),
      web_contents_(web_contents->GetWeakPtr()),
      controller_common_(std::move(controller_common)) {}

AutofillKeyboardAccessoryControllerImpl::
    ~AutofillKeyboardAccessoryControllerImpl() = default;

void AutofillKeyboardAccessoryControllerImpl::Hide(
    SuggestionHidingReason reason) {
  // For tests, keep open when hiding is due to external stimuli.
  if (keep_popup_open_for_testing_ &&
      (reason == SuggestionHidingReason::kWidgetChanged ||
       reason == SuggestionHidingReason::kEndEditing)) {
    // Don't close the popup because the browser window is resized or because
    // too many fields get focus one after each other (this can happen on
    // Desktop, if multiple password forms are present, and they are all
    // autofilled by default).
    return;
  }

  if (delegate_) {
    delegate_->ClearPreviewedForm();
    delegate_->OnSuggestionsHidden();
  }
  popup_hide_helper_.reset();
  AutofillMetrics::LogAutofillSuggestionHidingReason(
      suggestions_filling_product_, reason);
  HideViewAndDie();
}

void AutofillKeyboardAccessoryControllerImpl::HideViewAndDie() {
  // Invalidates in particular ChromeAutofillClient's WeakPtr to `this`, which
  // prevents recursive calls triggered by `view_->Hide()`
  // (crbug.com/1267047).
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Mark the popup-like filling sources as unavailable.
  // Note: We don't invoke ManualFillingController::Hide() here, as we might
  // switch between text input fields.
  if (web_contents_) {
    if (base::WeakPtr<ManualFillingController> manual_filling_controller =
            ManualFillingController::GetOrCreate(web_contents_.get())) {
      manual_filling_controller->UpdateSourceAvailability(
          FillingSource::AUTOFILL,
          /*has_suggestions=*/false);
    }
  }

  // TODO(crbug.com/1341374, crbug.com/1277218): Move this into the asynchronous
  // call?
  if (view_) {
    view_->Hide();
    view_.reset();
  }

  if (self_deletion_weak_ptr_factory_.HasWeakPtrs()) {
    return;
  }

  // TODO: Examine whether this is really enough or revert to the one from
  // the popup controller.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<AutofillKeyboardAccessoryControllerImpl> weak_this) {
            delete weak_this.get();
          },
          self_deletion_weak_ptr_factory_.GetWeakPtr()));
}

void AutofillKeyboardAccessoryControllerImpl::ViewDestroyed() {
  Hide(SuggestionHidingReason::kViewDestroyed);
}

gfx::NativeView AutofillKeyboardAccessoryControllerImpl::container_view()
    const {
  return controller_common_.container_view;
}

content::WebContents* AutofillKeyboardAccessoryControllerImpl::GetWebContents()
    const {
  return web_contents_.get();
}

const gfx::RectF& AutofillKeyboardAccessoryControllerImpl::element_bounds()
    const {
  return controller_common_.element_bounds;
}

PopupAnchorType AutofillKeyboardAccessoryControllerImpl::anchor_type() const {
  return controller_common_.anchor_type;
}

base::i18n::TextDirection
AutofillKeyboardAccessoryControllerImpl::GetElementTextDirection() const {
  return controller_common_.text_direction;
}

void AutofillKeyboardAccessoryControllerImpl::OnSuggestionsChanged() {
  // Assume that suggestions are (still) available. If this is wrong, the method
  // `HideViewAndDie` will be called soon after and will hide all suggestions.
  if (base::WeakPtr<ManualFillingController> manual_filling_controller =
          ManualFillingController::GetOrCreate(web_contents_.get())) {
    manual_filling_controller->UpdateSourceAvailability(
        FillingSource::AUTOFILL,
        /*has_suggestions=*/true);
  }
  if (view_) {
    view_->Show();
  }
}

void AutofillKeyboardAccessoryControllerImpl::AcceptSuggestion(
    int index,
    autofill::AutofillMetrics::SuggestionAcceptedMethod accept_method) {
  // Ignore clicks immediately after the popup was shown. This is to prevent
  // users accidentally accepting suggestions (crbug.com/1279268).
  if (!barrier_for_accepting_.value() && !disable_threshold_for_testing_) {
    return;
  }

  if (base::checked_cast<size_t>(index) >= suggestions_.size() ||
      !IsAcceptableSuggestionType(suggestions_[index].type)) {
    // Prevents crashes from crbug.com/521133. It seems that in rare cases or
    // races the suggestions_ and the user-selected index may be out of sync.
    // If the index points out of bounds, Chrome will crash. Prevent this by
    // ignoring the selection and wait for another signal from the user.
    return;
  }
  if (IsPointerLocked(web_contents_.get())) {
    Hide(SuggestionHidingReason::kMouseLocked);
    return;
  }

  // Use a copy instead of a reference here. Under certain circumstances,
  // `DidAcceptSuggestion()` invalidate the reference.
  Suggestion suggestion = suggestions_[index];
  if (!suggestion.IsAcceptable()) {
    return;
  }

  if (base::WeakPtr<ManualFillingController> manual_filling_controller =
          ManualFillingController::GetOrCreate(web_contents_.get())) {
    // Accepting a suggestion should hide all suggestions. To prevent them from
    // coming up in Multi-Window mode, mark the source as unavailable.
    manual_filling_controller->UpdateSourceAvailability(
        FillingSource::AUTOFILL,
        /*has_suggestions=*/false);
    manual_filling_controller->Hide();
  }

  NotifyUserEducationAboutAcceptedSuggestion(web_contents_.get(), suggestion);
  if (suggestion.acceptance_a11y_announcement && view_) {
    view_->AxAnnounce(*suggestion.acceptance_a11y_announcement);
  }

  base::UmaHistogramEnumeration("Autofill.SuggestionAccepted.Method",
                                accept_method);
  delegate_->DidAcceptSuggestion(
      suggestion, AutofillSuggestionDelegate::SuggestionMetadata{.row = index});
}

bool AutofillKeyboardAccessoryControllerImpl::RemoveSuggestion(
    int index,
    AutofillMetrics::SingleEntryRemovalMethod removal_method) {
  CHECK_EQ(removal_method,
           AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory);
  RemovalConfirmationText removal_text;
  if (!GetRemovalConfirmationText(index, &removal_text)) {
    return false;
  }

  view_->ConfirmDeletion(
      removal_text.title, removal_text.body, removal_text.body_link,
      removal_text.confirm_button_text,
      base::BindOnce(
          &AutofillKeyboardAccessoryControllerImpl::OnDeletionDialogClosed,
          GetWeakPtr(), index));
  return true;
}

void AutofillKeyboardAccessoryControllerImpl::OnDeletionDialogClosed(
    int index,
    bool confirmed) {
  // This function might be called in a callback, so ensure the list index is
  // still in bounds. If not, terminate the removing and consider it failed.
  // TODO(crbug.com/40766704): Replace these checks with a stronger identifier.
  if (base::checked_cast<size_t>(index) >= suggestions_.size()) {
    return;
  }
  CHECK_EQ(suggestions_.size(), labels_.size());

  const FillingProduct filling_product =
      GetFillingProductFromSuggestionType(GetSuggestionAt(index).type);

  if (filling_product == FillingProduct::kAddress && web_contents_) {
    PersonalDataManager* pdm = PersonalDataManagerFactory::GetForBrowserContext(
        web_contents_->GetBrowserContext());

    const auto* payload = std::get_if<Suggestion::AutofillProfilePayload>(
        &GetSuggestionAt(index).payload);
    if (pdm && payload) {
      const AutofillProfile* profile =
          pdm->address_data_manager().GetProfileByGUID(payload->guid.value());
      if (profile) {
        AutofillMetrics::LogDeleteAddressProfileFromKeyboardAccessory(
            confirmed, profile->record_type());
      }
    }
  }

  if (!confirmed) {
    return;
  }

  if (!delegate_->RemoveSuggestion(suggestions_[index])) {
    return;
  }
  switch (filling_product) {
    case FillingProduct::kAddress:
      // Address metrics are recorded earlier in this function because they are
      // recorded even if user canceled the dialog.
      break;
    case FillingProduct::kAutocomplete:
      AutofillMetrics::OnAutocompleteSuggestionDeleted(
          AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory);
      if (view_) {
        view_->AxAnnounce(l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_AUTOCOMPLETE_ENTRY_DELETED_A11Y_HINT,
            suggestions_[index].main_text.value));
      }
      break;
    case FillingProduct::kCreditCard:
      // TODO(crbug.com/41482065): Add metrics for credit cards.
      break;
    case FillingProduct::kNone:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kIban:
    case FillingProduct::kPassword:
    case FillingProduct::kPasskey:
    case FillingProduct::kCompose:
    case FillingProduct::kPlusAddresses:
    case FillingProduct::kAutofillAi:
    case FillingProduct::kLoyaltyCard:
    case FillingProduct::kIdentityCredential:
    case FillingProduct::kDataList:
    case FillingProduct::kOneTimePassword:
      break;
  }

  // Remove the deleted element.
  suggestions_.erase(suggestions_.begin() + index);
  labels_.erase(labels_.begin() + index);

  if (HasSuggestions()) {
    delegate_->ClearPreviewedForm();
    OnSuggestionsChanged();
  } else {
    Hide(SuggestionHidingReason::kNoSuggestions);
  }
}

int AutofillKeyboardAccessoryControllerImpl::GetLineCount() const {
  return suggestions_.size();
}

const std::vector<Suggestion>&
AutofillKeyboardAccessoryControllerImpl::GetSuggestions() const {
  return suggestions_;
}

const Suggestion& AutofillKeyboardAccessoryControllerImpl::GetSuggestionAt(
    int row) const {
  return suggestions_[row];
}

FillingProduct AutofillKeyboardAccessoryControllerImpl::GetMainFillingProduct()
    const {
  return delegate_->GetMainFillingProduct();
}

std::optional<AutofillClient::PopupScreenLocation>
AutofillKeyboardAccessoryControllerImpl::GetPopupScreenLocation() const {
  return std::nullopt;
}

void AutofillKeyboardAccessoryControllerImpl::Show(
    UiSessionId ui_session_id,
    std::vector<Suggestion> suggestions,
    AutofillSuggestionTriggerSource trigger_source,
    AutoselectFirstSuggestion autoselect_first_suggestion) {
  ui_session_id_ = ui_session_id;
  suggestions_filling_product_ =
      !suggestions.empty() && IsStandaloneSuggestionType(suggestions[0].type)
          ? GetFillingProductFromSuggestionType(suggestions[0].type)
          : FillingProduct::kNone;
  if (auto* rwhv = web_contents_->GetRenderWidgetHostView();
      !rwhv || !rwhv->HasFocus()) {
    Hide(SuggestionHidingReason::kNoFrameHasFocus);
    return;
  }

  // The focused frame may be a different frame than the one the delegate is
  // associated with. This happens in two scenarios:
  // - With frame-transcending forms: the focused frame is subframe, whose
  //   form has been flattened into an ancestor form.
  // - With race conditions: while Autofill parsed the form, the focused may
  //   have moved to another frame.
  // We support the case where the focused frame is a descendant of the
  // `delegate_`'s frame. We observe the focused frame's RenderFrameDeleted()
  // event.
  content::RenderFrameHost* rfh = web_contents_->GetFocusedFrame();
  if (!rfh || !delegate_ ||
      !IsAncestorOf(GetRenderFrameHost(*delegate_), rfh)) {
    Hide(SuggestionHidingReason::kNoFrameHasFocus);
    return;
  }

  if (IsPointerLocked(web_contents_.get())) {
    Hide(SuggestionHidingReason::kMouseLocked);
    return;
  }

  AutofillPopupHideHelper::HidingParams hiding_params = {
      .hide_on_web_contents_lost_focus = true};
  AutofillPopupHideHelper::HidingCallback hiding_callback = base::BindRepeating(
      &AutofillKeyboardAccessoryControllerImpl::Hide, base::Unretained(this));
  // TODO(crbug.com/40280362): Implement PIP hiding for Android.
  popup_hide_helper_.emplace(
      web_contents_.get(), rfh->GetGlobalId(), std::move(hiding_params),
      std::move(hiding_callback),
      /*pip_detection_callback=*/base::BindRepeating([] { return false; }));

  suggestions_ = std::move(suggestions);
  OrderSuggestionsAndCreateLabels();
  trigger_source_ = trigger_source;

  if (view_) {
    OnSuggestionsChanged();
  } else {
    view_ = AutofillKeyboardAccessoryView::Create(GetWeakPtr());
    // It is possible to fail to create the accessory view.
    if (!view_) {
      Hide(SuggestionHidingReason::kViewDestroyed);
      return;
    }

    if (base::WeakPtr<ManualFillingController> manual_filling_controller =
            ManualFillingController::GetOrCreate(web_contents_.get())) {
      manual_filling_controller->UpdateSourceAvailability(
          FillingSource::AUTOFILL, !suggestions_.empty());
    }
    if (view_) {
      view_->Show();
    }
  }

  barrier_for_accepting_ = NextIdleBarrier::CreateNextIdleBarrierWithDelay(
      kIgnoreEarlyClicksOnSuggestionsDuration);
  // TODO(crbug.com/364165357): Use actually shown suggestions.
  delegate_->OnSuggestionsShown(suggestions_);
}

std::optional<AutofillSuggestionController::UiSessionId>
AutofillKeyboardAccessoryControllerImpl::GetUiSessionId() const {
  return view_ ? std::make_optional(ui_session_id_) : std::nullopt;
}

void AutofillKeyboardAccessoryControllerImpl::SetKeepPopupOpenForTesting(
    bool keep_popup_open_for_testing) {
  keep_popup_open_for_testing_ = keep_popup_open_for_testing;
}

void AutofillKeyboardAccessoryControllerImpl::UpdateDataListValues(
    base::span<const SelectOption> options) {
  suggestions_ =
      UpdateSuggestionsFromDataList(options, std::move(suggestions_));
  OrderSuggestionsAndCreateLabels();
  if (HasSuggestions()) {
    OnSuggestionsChanged();
  } else {
    Hide(SuggestionHidingReason::kNoSuggestions);
  }
}

bool AutofillKeyboardAccessoryControllerImpl::HasSuggestions() const {
  return !suggestions_.empty() &&
         IsStandaloneSuggestionType(suggestions_[0].type);
}

// AutofillKeyboardAccessoryController implementation:

std::vector<std::vector<Suggestion::Text>>
AutofillKeyboardAccessoryControllerImpl::GetSuggestionLabelsAt(int row) const {
  CHECK_LT(base::checked_cast<size_t>(row), labels_.size());
  return {{labels_[row]}};
}

bool AutofillKeyboardAccessoryControllerImpl::GetRemovalConfirmationText(
    int index,
    RemovalConfirmationText* removal_text) {
  CHECK_LT(base::checked_cast<size_t>(index), suggestions_.size());
  const std::u16string& value = suggestions_[index].main_text.value;
  const SuggestionType type = suggestions_[index].type;
  const Suggestion::Payload& payload = suggestions_[index].payload;

  if (type == SuggestionType::kAutocompleteEntry) {
    return GetAutocompleteRemovalText(value, removal_text);
  }

  if (type == SuggestionType::kCreditCardEntry) {
    return GetCreditCardRemovalText(payload, web_contents_.get(), removal_text);
  }

  if (type == SuggestionType::kAddressEntry) {
    return GetAddressRemovalText(payload, value, web_contents_.get(),
                                 removal_text);
  }

  return false;
}

void AutofillKeyboardAccessoryControllerImpl::
    OrderSuggestionsAndCreateLabels() {
  // If there is an Undo suggestion, move it to the front.
  if (auto it = std::ranges::find(suggestions_, SuggestionType::kUndoOrClear,
                                  &Suggestion::type);
      it != suggestions_.end()) {
    std::rotate(suggestions_.begin(), it, it + 1);
  }

  labels_.clear();
  labels_.reserve(suggestions_.size());
  for (const Suggestion& suggestion : suggestions_) {
    labels_.push_back(CreateLabel(suggestion));
  }
}

}  // namespace autofill
