// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_controller_impl.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "chrome/browser/password_manager/android/access_loss/password_access_loss_warning_bridge_impl.h"
#include "chrome/browser/password_manager/android/local_passwords_migration_warning_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_view.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_utils.h"
#include "chrome/browser/ui/autofill/next_idle_barrier.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/metrics/granular_filling_metrics.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

using FillingSource = ManualFillingController::FillingSource;

constexpr std::u16string_view kLabelSeparator = u" ";
constexpr size_t kMaxBulletCount = 8;

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
  if (GetFillingProductFromSuggestionType(suggestion.type) ==
      FillingProduct::kPassword) {
    // The `Suggestion::labels` can never be empty since it must contain a
    // password.
    const std::u16string password =
        suggestion.labels[0][0].value.substr(0, kMaxBulletCount);

    // The `Suggestion::additional_label` contains the signon_realm or is empty.
    if (suggestion.additional_label.empty()) {
      return Suggestion::Text(password);
    }
    return Suggestion::Text(
        base::StrCat({suggestion.additional_label, kLabelSeparator, password}));
  }

  return Suggestion::Text(suggestion.labels[0][0].value);
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
      delegate, web_contents, std::move(controller_common),
      base::BindRepeating(&local_password_migration::ShowWarning));
  return controller->GetWeakPtr();
}

AutofillKeyboardAccessoryControllerImpl::
    AutofillKeyboardAccessoryControllerImpl(
        base::WeakPtr<AutofillSuggestionDelegate> delegate,
        content::WebContents* web_contents,
        PopupControllerCommon controller_common,
        ShowPasswordMigrationWarningCallback
            show_pwd_migration_warning_callback)
    : delegate_(delegate),
      web_contents_(web_contents->GetWeakPtr()),
      controller_common_(std::move(controller_common)),
      show_pwd_migration_warning_callback_(
          std::move(show_pwd_migration_warning_callback)) {}

AutofillKeyboardAccessoryControllerImpl::
    ~AutofillKeyboardAccessoryControllerImpl() = default;

void AutofillKeyboardAccessoryControllerImpl::Hide(
    SuggestionHidingReason reason) {
  // If the reason for hiding is only stale data or a user interacting with
  // native Chrome UI (kFocusChanged/kEndEditing), the popup might be kept open.
  if (is_view_pinned_ && (reason == SuggestionHidingReason::kStaleData ||
                          reason == SuggestionHidingReason::kFocusChanged ||
                          reason == SuggestionHidingReason::kEndEditing)) {
    return;  // Don't close the popup while waiting for an update.
  }
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

void AutofillKeyboardAccessoryControllerImpl::AcceptSuggestion(int index) {
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
  if (!suggestion.is_acceptable) {
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

  delegate_->DidAcceptSuggestion(
      suggestion, AutofillSuggestionDelegate::SuggestionMetadata{.row = index});

  if (suggestion.type != SuggestionType::kPasswordEntry) {
    // Returning early because the code below triggers the UI which is shown
    // after accepting passwords.
    return;
  }
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning)) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    if (!access_loss_warning_bridge_) {
      access_loss_warning_bridge_ =
          std::make_unique<PasswordAccessLossWarningBridgeImpl>();
    }
    if (profile && access_loss_warning_bridge_->ShouldShowAccessLossNoticeSheet(
                       profile->GetPrefs(), /*called_at_startup=*/false)) {
      access_loss_warning_bridge_->MaybeShowAccessLossNoticeSheet(
          profile->GetPrefs(), web_contents_->GetTopLevelNativeWindow(),
          profile, /*called_at_startup=*/false,
          password_manager_android_util::PasswordAccessLossWarningTriggers::
              kKeyboardAcessoryBar);
    }
  }
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsMigrationWarning)) {
    show_pwd_migration_warning_callback_.Run(
        web_contents_->GetTopLevelNativeWindow(),
        Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
        password_manager::metrics_util::PasswordMigrationWarningTriggers::
            kKeyboardAcessoryBar);
  }
}

bool AutofillKeyboardAccessoryControllerImpl::RemoveSuggestion(
    int index,
    AutofillMetrics::SingleEntryRemovalMethod removal_method) {
  CHECK_EQ(removal_method,
           AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory);
  std::u16string title;
  std::u16string body;
  if (!GetRemovalConfirmationText(index, &title, &body)) {
    return false;
  }

  view_->ConfirmDeletion(
      title, body,
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
  if (!confirmed) {
    if (filling_product == FillingProduct::kAddress) {
      autofill_metrics::LogDeleteAddressProfileFromExtendedMenu(
          /*user_accepted_delete=*/false);
    }
    return;
  }

  if (!delegate_->RemoveSuggestion(suggestions_[index])) {
    return;
  }
  switch (filling_product) {
    case FillingProduct::kAddress:
      AutofillMetrics::LogDeleteAddressProfileFromKeyboardAccessory();
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
    case FillingProduct::kStandaloneCvc:
      // TODO(crbug.com/41482065): Add metrics for credit cards.
      break;
    case FillingProduct::kNone:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kIban:
    case FillingProduct::kPassword:
    case FillingProduct::kCompose:
    case FillingProduct::kPlusAddresses:
    case FillingProduct::kPredictionImprovements:
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

void AutofillKeyboardAccessoryControllerImpl::PinView() {
  is_view_pinned_ = true;
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
    std::u16string* title,
    std::u16string* body) {
  CHECK_LT(base::checked_cast<size_t>(index), suggestions_.size());
  const std::u16string& value = suggestions_[index].main_text.value;
  const SuggestionType type = suggestions_[index].type;
  const Suggestion::BackendId backend_id =
      suggestions_[index].GetPayload<Suggestion::BackendId>();

  if (type == SuggestionType::kAutocompleteEntry) {
    if (title) {
      title->assign(value);
    }
    if (body) {
      body->assign(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_CONFIRMATION_BODY));
    }
    return true;
  }

  if (type != SuggestionType::kAddressEntry &&
      type != SuggestionType::kCreditCardEntry) {
    return false;
  }
  PersonalDataManager* pdm = PersonalDataManagerFactory::GetForBrowserContext(
      web_contents_->GetBrowserContext());

  if (const CreditCard* credit_card =
          pdm->payments_data_manager().GetCreditCardByGUID(
              absl::get<Suggestion::Guid>(backend_id).value())) {
    if (!CreditCard::IsLocalCard(credit_card)) {
      return false;
    }
    if (title) {
      title->assign(credit_card->CardNameAndLastFourDigits());
    }
    if (body) {
      body->assign(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DELETE_CREDIT_CARD_SUGGESTION_CONFIRMATION_BODY));
    }
    return true;
  }

  if (const AutofillProfile* profile =
          pdm->address_data_manager().GetProfileByGUID(
              absl::get<Suggestion::Guid>(backend_id).value())) {
    if (title) {
      std::u16string street_address = profile->GetRawInfo(ADDRESS_HOME_CITY);
      if (!street_address.empty()) {
        title->swap(street_address);
      } else {
        title->assign(value);
      }
    }
    if (body) {
      body->assign(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DELETE_PROFILE_SUGGESTION_CONFIRMATION_BODY));
    }

    return true;
  }

  return false;  // The ID was valid. The entry may have been deleted in a race.
}

void AutofillKeyboardAccessoryControllerImpl::
    OrderSuggestionsAndCreateLabels() {
  // If there is an Undo suggestion, move it to the front.
  if (auto it = base::ranges::find(suggestions_, SuggestionType::kUndoOrClear,
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
