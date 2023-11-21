// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_active_popup.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/autofill/manual_filling_controller_impl.h"
#include "chrome/browser/password_manager/android/local_passwords_migration_warning_util.h"
#include "components/password_manager/core/common/password_manager_features.h"

using FillingSource = ManualFillingController::FillingSource;
#endif

using base::WeakPtr;

namespace autofill {

namespace {

// The duration for which clicks on the just-shown Autofill popup should be
// ignored. This is to prevent users accidentally accepting suggestions
// (crbug.com/1279268).
static constexpr base::TimeDelta kIgnoreEarlyClicksOnPopupDuration =
    base::Milliseconds(500);

// Returns true if the given id refers to an element that can be accepted.
bool CanAccept(PopupItemId id) {
  return id != PopupItemId::kSeparator &&
         id != PopupItemId::kInsecureContextPaymentDisabledMessage &&
         id != PopupItemId::kMixedFormMessage && id != PopupItemId::kTitle;
}

}  // namespace

#if !BUILDFLAG(IS_MAC)
// static
WeakPtr<AutofillPopupControllerImpl> AutofillPopupControllerImpl::GetOrCreate(
    WeakPtr<AutofillPopupControllerImpl> previous,
    WeakPtr<AutofillPopupDelegate> delegate,
    content::WebContents* web_contents,
    gfx::NativeView container_view,
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction) {
  if (previous && previous->delegate_.get() == delegate.get() &&
      previous->container_view() == container_view) {
    if (previous->self_deletion_weak_ptr_factory_.HasWeakPtrs())
      previous->self_deletion_weak_ptr_factory_.InvalidateWeakPtrs();
    previous->SetElementBounds(element_bounds);
    previous->ClearState();
    return previous;
  }

  if (previous)
    previous->Hide(PopupHidingReason::kViewDestroyed);
#if BUILDFLAG(IS_ANDROID)
  AutofillPopupControllerImpl* controller = new AutofillPopupControllerImpl(
      delegate, web_contents, container_view, element_bounds, text_direction,
      base::BindRepeating(&local_password_migration::ShowWarning),
      /*parent=*/absl::nullopt);
#else
  AutofillPopupControllerImpl* controller = new AutofillPopupControllerImpl(
      delegate, web_contents, container_view, element_bounds, text_direction,
      base::DoNothing(), /*parent=*/absl::nullopt);
#endif
  return controller->GetWeakPtr();
}
#endif

AutofillPopupControllerImpl::AutofillPopupControllerImpl(
    base::WeakPtr<AutofillPopupDelegate> delegate,
    content::WebContents* web_contents,
    gfx::NativeView container_view,
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    base::RepeatingCallback<
        void(gfx::NativeWindow,
             Profile*,
             password_manager::metrics_util::PasswordMigrationWarningTriggers)>
        show_pwd_migration_warning_callback,
    absl::optional<base::WeakPtr<ExpandablePopupParentControllerImpl>> parent)
    : content::WebContentsObserver(web_contents),
      controller_common_(element_bounds, text_direction, container_view),
      delegate_(delegate),
      show_pwd_migration_warning_callback_(
          std::move(show_pwd_migration_warning_callback)),
      parent_controller_(parent) {
  ClearState();
  delegate->RegisterDeletionCallback(base::BindOnce(
      &AutofillPopupControllerImpl::HideViewAndDie, GetWeakPtr()));
  PictureInPictureWindowManager* picture_in_picture_window_manager =
      PictureInPictureWindowManager::GetInstance();
  CHECK(picture_in_picture_window_manager);
  picture_in_picture_window_observation_.Observe(
      picture_in_picture_window_manager);
}

AutofillPopupControllerImpl::~AutofillPopupControllerImpl() = default;

void AutofillPopupControllerImpl::RenderFrameDeleted(
    content::RenderFrameHost* rfh) {
  // If the popup menu has been triggered from within an iframe and that frame
  // is deleted, hide the popup. This is necessary because the popup may
  // actually be shown by the AutofillExternalDelegate of an ancestor frame,
  // which is not notified about `rfh`'s destruction and therefore won't close
  // the popup.
  if (key_press_observer_.handler &&
      key_press_observer_.rfh == rfh->GetGlobalId()) {
    Hide(PopupHidingReason::kRendererEvent);
  }
}

void AutofillPopupControllerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (key_press_observer_.handler &&
      key_press_observer_.rfh ==
          navigation_handle->GetPreviousRenderFrameHostId() &&
      !navigation_handle->IsSameDocument()) {
    Hide(PopupHidingReason::kNavigation);
  }
}

void AutofillPopupControllerImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN) {
    Hide(PopupHidingReason::kTabGone);
  }
}

void AutofillPopupControllerImpl::Show(
    std::vector<Suggestion> suggestions,
    AutofillSuggestionTriggerSource trigger_source,
    AutoselectFirstSuggestion autoselect_first_suggestion) {
  if (IsMouseLocked()) {
    Hide(PopupHidingReason::kMouseLocked);
    return;
  }

  content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame();
  if (!rfh) {
    Hide(PopupHidingReason::kNoFrameHasFocus);
    return;
  }

  SetSuggestions(std::move(suggestions));

  trigger_source_ = trigger_source;
  should_ignore_mouse_observed_outside_item_bounds_check_ =
      trigger_source_ ==
      AutofillSuggestionTriggerSource::kManualFallbackAddress;

  if (view_) {
    OnSuggestionsChanged();
  } else {
    bool has_parent = parent_controller_ && parent_controller_->get();
    view_ = has_parent
                ? parent_controller_->get()->CreateSubPopupView(GetWeakPtr())
                : AutofillPopupView::Create(GetWeakPtr());

    // It is possible to fail to create the popup, in this case
    // treat the popup as hiding right away.
    if (!view_) {
      Hide(PopupHidingReason::kViewDestroyed);
      return;
    }

#if BUILDFLAG(IS_ANDROID)
    ManualFillingController::GetOrCreate(web_contents())
        ->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                   !suggestions_.empty());
#endif
    if (!view_ || !view_->Show(autoselect_first_suggestion)) {
      return;
    }

    // We only fire the event when a new popup shows. We do not fire the
    // event when suggestions changed.
    FireControlsChangedEvent(true);
  }
  time_view_shown_ = base::TimeTicks::Now();

  if (IsRootPopup()) {
    key_press_observer_.rfh = rfh->GetGlobalId();
    key_press_observer_.handler = base::BindRepeating(
        // Cannot bind HandleKeyPressEvent() directly because of its
        // return value.
        [](base::WeakPtr<AutofillPopupControllerImpl> weak_this,
           const content::NativeWebKeyboardEvent& event) {
          return weak_this && weak_this->HandleKeyPressEvent(event);
        },
        GetWeakPtr());
    rfh->GetRenderWidgetHost()->AddKeyPressEventCallback(
        key_press_observer_.handler);

    delegate_->OnPopupShown();
  }
}

AutofillSuggestionTriggerSource
AutofillPopupControllerImpl::GetAutofillSuggestionTriggerSource() const {
  return trigger_source_;
}

bool AutofillPopupControllerImpl::
    ShouldIgnoreMouseObservedOutsideItemBoundsCheck() const {
  return should_ignore_mouse_observed_outside_item_bounds_check_ ||
         !IsRootPopup() ||
         base::FeatureList::IsEnabled(
             features::kAutofillPopupDisablePaintChecks);
}

void AutofillPopupControllerImpl::UpdateDataListValues(
    base::span<const SelectOption> options) {
  // Remove all the old data list values, which should always be at the top of
  // the list if they are present.
  while (!suggestions_.empty() &&
         suggestions_[0].popup_item_id == PopupItemId::kDatalistEntry) {
    suggestions_.erase(suggestions_.begin());
  }

  // If there are no new data list values, exit (clearing the separator if there
  // is one).
  if (options.empty()) {
    if (!suggestions_.empty() &&
        suggestions_[0].popup_item_id == PopupItemId::kSeparator) {
      suggestions_.erase(suggestions_.begin());
    }

    // The popup contents have changed, so either update the bounds or hide it.
    if (HasSuggestions())
      OnSuggestionsChanged();
    else
      Hide(PopupHidingReason::kNoSuggestions);

    return;
  }

  // Add a separator if there are any other values.
  if (!suggestions_.empty() &&
      suggestions_[0].popup_item_id != PopupItemId::kSeparator) {
    suggestions_.insert(suggestions_.begin(),
                        Suggestion(PopupItemId::kSeparator));
  }

  // Prepend the parameters to the suggestions we already have.
  suggestions_.insert(suggestions_.begin(), options.size(), Suggestion());
  for (size_t i = 0; i < options.size(); i++) {
    suggestions_[i].main_text =
        Suggestion::Text(options[i].value, Suggestion::Text::IsPrimary(true));
    suggestions_[i].labels = {{Suggestion::Text(options[i].content)}};
    suggestions_[i].popup_item_id = PopupItemId::kDatalistEntry;
  }

  OnSuggestionsChanged();
}

void AutofillPopupControllerImpl::PinView() {
  is_view_pinned_ = true;
}

void AutofillPopupControllerImpl::Hide(PopupHidingReason reason) {
  // If the reason for hiding is only stale data or a user interacting with
  // native Chrome UI (kFocusChanged/kEndEditing), the popup might be kept open.
  if (is_view_pinned_ && (reason == PopupHidingReason::kStaleData ||
                          reason == PopupHidingReason::kFocusChanged ||
                          reason == PopupHidingReason::kEndEditing)) {
    return;  // Don't close the popup while waiting for an update.
  }
  // For tests, keep open when hiding is due to external stimuli.
  if (keep_popup_open_for_testing_ &&
      reason == PopupHidingReason::kWidgetChanged) {
    return;  // Don't close the popup because the browser window is resized.
  }

  if (delegate_ && IsRootPopup()) {
    delegate_->ClearPreviewedForm();
    delegate_->OnPopupHidden();
  }
  if (key_press_observer_.handler) {
    if (auto* rfh = content::RenderFrameHost::FromID(key_press_observer_.rfh)) {
      rfh->GetRenderWidgetHost()->RemoveKeyPressEventCallback(
          key_press_observer_.handler);
    }
    key_press_observer_ = {};
  }
  AutofillMetrics::LogAutofillPopupHidingReason(reason);
  HideViewAndDie();
}

void AutofillPopupControllerImpl::ViewDestroyed() {
  // The view has already been destroyed so clear the reference to it.
  view_ = nullptr;
  Hide(PopupHidingReason::kViewDestroyed);
}

bool AutofillPopupControllerImpl::HandleKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  if (sub_popup_controller_ &&
      sub_popup_controller_->HandleKeyPressEvent(event)) {
    return true;
  }

  return view_ && view_->HandleKeyPressEvent(event);
}

void AutofillPopupControllerImpl::OnSuggestionsChanged() {
#if BUILDFLAG(IS_ANDROID)
  // Assume that suggestions are (still) available. If this is wrong, the method
  // |HideViewAndDie| will be called soon after and will hide all suggestions.
  ManualFillingController::GetOrCreate(web_contents())
      ->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                 /*has_suggestions=*/true);
#endif

  if (view_) {
    view_->OnSuggestionsChanged();
  }
}

void AutofillPopupControllerImpl::AcceptSuggestion(int index,
                                                   base::TimeTicks event_time) {
  // Ignore clicks immediately after the popup was shown. This is to prevent
  // users accidentally accepting suggestions (crbug.com/1279268).
  CHECK(!time_view_shown_.is_null());
  CHECK(!event_time.is_null());
  const base::TimeDelta time_elapsed = event_time - time_view_shown_;
  if ((time_elapsed < kIgnoreEarlyClicksOnPopupDuration) &&
      !disable_threshold_for_testing_) {
    base::UmaHistogramCustomTimes(
        "Autofill.Popup.AcceptanceDelayThresholdNotMet", time_elapsed,
        base::Milliseconds(0), kIgnoreEarlyClicksOnPopupDuration,
        /*buckets=*/50);
    return;
  }

  if (static_cast<size_t>(index) >= suggestions_.size()) {
    // Prevents crashes from crbug.com/521133. It seems that in rare cases or
    // races the suggestions_ and the user-selected index may be out of sync.
    // If the index points out of bounds, Chrome will crash. Prevent this by
    // ignoring the selection and wait for another signal from the user.
    return;
  }

  if (IsMouseLocked()) {
    Hide(PopupHidingReason::kMouseLocked);
    return;
  }

  // Use a copy instead of a reference here. Under certain circumstances,
  // `DidAcceptSuggestion()` can call `SetSuggestions()` and invalidate the
  // reference.
  Suggestion suggestion = suggestions_[index];
#if BUILDFLAG(IS_ANDROID)
  auto mf_controller = ManualFillingController::GetOrCreate(web_contents());
  // Accepting a suggestion should hide all suggestions. To prevent them from
  // coming up in Multi-Window mode, mark the source as unavailable.
  mf_controller->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                          /*has_suggestions=*/false);
  mf_controller->Hide();

#endif
  if (suggestion.popup_item_id == PopupItemId::kVirtualCreditCardEntry) {
    std::string event_name =
        suggestion.feature_for_iph ==
                feature_engagement::kIPHAutofillVirtualCardCVCSuggestionFeature
                    .name
            ? "autofill_virtual_card_cvc_suggestion_accepted"
            : "autofill_virtual_card_suggestion_accepted";
    feature_engagement::TrackerFactory::GetForBrowserContext(
        web_contents()->GetBrowserContext())
        ->NotifyEvent(event_name);
  }

  if (suggestion.feature_for_iph ==
      feature_engagement::kIPHAutofillExternalAccountProfileSuggestionFeature
          .name) {
    feature_engagement::TrackerFactory::GetForBrowserContext(
        web_contents()->GetBrowserContext())
        ->NotifyEvent("autofill_external_account_profile_suggestion_accepted");
  }

  absl::optional<std::u16string> announcement =
      suggestion.acceptance_a11y_announcement;
  if (announcement && view_) {
    view_->AxAnnounce(*announcement);
  }

  delegate_->DidAcceptSuggestion(
      suggestion,
      AutofillPopupDelegate::SuggestionPosition{
          .row = index, .sub_popup_level = GetPopupLevel()},
      trigger_source_);
#if BUILDFLAG(IS_ANDROID)
  if ((suggestion.popup_item_id == PopupItemId::kPasswordEntry ||
       suggestion.popup_item_id == PopupItemId::kUsernameEntry) &&
      base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsMigrationWarning)) {
    show_pwd_migration_warning_callback_.Run(
        web_contents()->GetTopLevelNativeWindow(),
        Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
        password_manager::metrics_util::PasswordMigrationWarningTriggers::
            kKeyboardAcessoryBar);
  }
#endif
}

void AutofillPopupControllerImpl::PerformButtonActionForSuggestion(int index) {
  CHECK_LE(base::checked_cast<size_t>(index), suggestions_.size());
  delegate_->DidPerformButtonActionForSuggestion(suggestions_[index]);
}

gfx::NativeView AutofillPopupControllerImpl::container_view() const {
  return controller_common_.container_view;
}

content::WebContents* AutofillPopupControllerImpl::GetWebContents() const {
  return web_contents();
}

const gfx::RectF& AutofillPopupControllerImpl::element_bounds() const {
  return controller_common_.element_bounds;
}

void AutofillPopupControllerImpl::SetElementBounds(const gfx::RectF& bounds) {
  controller_common_.element_bounds.set_origin(bounds.origin());
  controller_common_.element_bounds.set_size(bounds.size());
}

base::i18n::TextDirection AutofillPopupControllerImpl::GetElementTextDirection()
    const {
  return controller_common_.text_direction;
}

std::vector<Suggestion> AutofillPopupControllerImpl::GetSuggestions() const {
  return suggestions_;
}

base::WeakPtr<AutofillPopupController>
AutofillPopupControllerImpl::OpenSubPopup(
    const gfx::RectF& anchor_bounds,
    std::vector<Suggestion> suggestions,
    AutoselectFirstSuggestion autoselect_first_suggestion) {
  AutofillPopupControllerImpl* controller = new AutofillPopupControllerImpl(
      delegate_, web_contents(), controller_common_.container_view,
      anchor_bounds, controller_common_.text_direction, base::DoNothing(),
      /*parent=*/GetWeakPtr());

  // Show() can fail and cause controller deletion. Therefore store the weak
  // pointer before, so that this method returns null when that happens.
  sub_popup_controller_ = controller->GetWeakPtr();
  controller->Show(std::move(suggestions), trigger_source_,
                   autoselect_first_suggestion);
  return sub_popup_controller_;
}

void AutofillPopupControllerImpl::HideSubPopup() {
  if (sub_popup_controller_) {
    sub_popup_controller_->Hide(
        PopupHidingReason::kExpandedSuggestionCollapsedSubPopup);
    sub_popup_controller_ = nullptr;
  }
}

bool AutofillPopupControllerImpl::IsRootPopup() const {
  return !parent_controller_;
}

void AutofillPopupControllerImpl::OnEnterPictureInPicture() {
  if (view_ && view_->OverlapsWithPictureInPictureWindow()) {
    Hide(PopupHidingReason::kOverlappingWithPictureInPictureWindow);
  }
}

int AutofillPopupControllerImpl::GetLineCount() const {
  return suggestions_.size();
}

const Suggestion& AutofillPopupControllerImpl::GetSuggestionAt(int row) const {
  return suggestions_[row];
}

std::u16string AutofillPopupControllerImpl::GetSuggestionMainTextAt(
    int row) const {
  return suggestions_[row].main_text.value;
}

std::u16string AutofillPopupControllerImpl::GetSuggestionMinorTextAt(
    int row) const {
  return suggestions_[row].minor_text.value;
}

std::vector<std::vector<Suggestion::Text>>
AutofillPopupControllerImpl::GetSuggestionLabelsAt(int row) const {
  return suggestions_[row].labels;
}

bool AutofillPopupControllerImpl::GetRemovalConfirmationText(
    int list_index,
    std::u16string* title,
    std::u16string* body) {
  const std::u16string& value = suggestions_[list_index].main_text.value;
  const PopupItemId popup_item_id = suggestions_[list_index].popup_item_id;
  const Suggestion::BackendId backend_id =
      suggestions_[list_index].GetPayload<Suggestion::BackendId>();

  if (popup_item_id == PopupItemId::kAutocompleteEntry) {
    if (title) {
      title->assign(value);
    }
    if (body) {
      body->assign(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_CONFIRMATION_BODY));
    }
    return true;
  }

  if (popup_item_id != PopupItemId::kAddressEntry &&
      popup_item_id != PopupItemId::kCreditCardEntry) {
    return false;
  }
  PersonalDataManager* pdm = PersonalDataManagerFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());

  if (const CreditCard* credit_card = pdm->GetCreditCardByGUID(
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

  if (const AutofillProfile* profile = pdm->GetProfileByGUID(
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

bool AutofillPopupControllerImpl::RemoveSuggestion(int list_index) {
  if (IsMouseLocked()) {
    Hide(PopupHidingReason::kMouseLocked);
    return false;
  }

  // This function might be called in a callback, so ensure the list index is
  // still in bounds. If not, terminate the removing and consider it failed.
  // TODO(crbug.com/1209792): Replace these checks with a stronger identifier.
  if (list_index < 0 || static_cast<size_t>(list_index) >= suggestions_.size())
    return false;

  PopupItemId suggestion_type = suggestions_[list_index].popup_item_id;
  if (!delegate_->RemoveSuggestion(
          suggestions_[list_index].main_text.value,
          suggestions_[list_index].popup_item_id,
          suggestions_[list_index].GetPayload<Suggestion::BackendId>())) {
    return false;
  }
  if (suggestion_type == PopupItemId::kAutocompleteEntry && view_) {
    view_->AxAnnounce(l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_AUTOCOMPLETE_ENTRY_DELETED_A11Y_HINT,
        suggestions_[list_index].main_text.value));
  }

  // Remove the deleted element.
  suggestions_.erase(suggestions_.begin() + list_index);

  if (HasSuggestions()) {
    delegate_->ClearPreviewedForm();
    should_ignore_mouse_observed_outside_item_bounds_check_ =
        suggestion_type == PopupItemId::kAutocompleteEntry;
    OnSuggestionsChanged();
  } else {
    Hide(PopupHidingReason::kNoSuggestions);
  }

  return true;
}

void AutofillPopupControllerImpl::SelectSuggestion(
    absl::optional<size_t> index) {
  if (IsMouseLocked()) {
    Hide(PopupHidingReason::kMouseLocked);
    return;
  }

  if (index) {
    DCHECK_LT(*index, suggestions_.size());
    if (!CanAccept(GetSuggestionAt(*index).popup_item_id)) {
      index = absl::nullopt;
    }
  }

  if (index) {
    delegate_->DidSelectSuggestion(GetSuggestionAt(*index), trigger_source_);
  } else {
    delegate_->ClearPreviewedForm();
  }
}

PopupType AutofillPopupControllerImpl::GetPopupType() const {
  return delegate_->GetPopupType();
}

std::optional<AutofillClient::PopupScreenLocation>
AutofillPopupControllerImpl::GetPopupScreenLocation() const {
  return view_ ? view_->GetPopupScreenLocation()
               : std::make_optional<AutofillClient::PopupScreenLocation>();
}

bool AutofillPopupControllerImpl::HasSuggestions() const {
  if (suggestions_.empty()) {
    return false;
  }
  PopupItemId popup_item_id = suggestions_[0].popup_item_id;
  return base::Contains(kItemsTriggeringFieldFilling, popup_item_id) ||
         popup_item_id == PopupItemId::kScanCreditCard;
}

void AutofillPopupControllerImpl::SetSuggestions(
    std::vector<Suggestion> suggestions) {
  suggestions_ = std::move(suggestions);
}

WeakPtr<AutofillPopupControllerImpl> AutofillPopupControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillPopupControllerImpl::ClearState() {
  // Don't clear view_, because otherwise the popup will have to get
  // regenerated and this will cause flickering.
  suggestions_.clear();
}

void AutofillPopupControllerImpl::HideViewAndDie() {
  HideSubPopup();

  // Invalidates in particular ChromeAutofillClient's WeakPtr to |this|, which
  // prevents recursive calls triggered by `view_->Hide()`
  // (crbug.com/1267047).
  weak_ptr_factory_.InvalidateWeakPtrs();

#if BUILDFLAG(IS_ANDROID)
  // Mark the popup-like filling sources as unavailable.
  // Note: We don't invoke ManualFillingController::Hide() here, as we might
  // switch between text input fields.
  ManualFillingController::GetOrCreate(web_contents())
      ->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                 /*has_suggestions=*/false);
#endif

  // TODO(crbug.com/1341374, crbug.com/1277218): Move this into the asynchronous
  // call?
  if (view_) {
    // We need to fire the event while view is not deleted yet.
    FireControlsChangedEvent(false);
    view_->Hide();
    view_ = nullptr;
  }

  if (self_deletion_weak_ptr_factory_.HasWeakPtrs())
    return;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](WeakPtr<AutofillPopupControllerImpl> weak_this) {
                       if (weak_this)
                         delete weak_this.get();
                     },
                     self_deletion_weak_ptr_factory_.GetWeakPtr()));
}

bool AutofillPopupControllerImpl::IsMouseLocked() const {
  content::RenderFrameHost* rfh;
  content::RenderWidgetHostView* rwhv;
  return web_contents() && (rfh = web_contents()->GetFocusedFrame()) &&
         (rwhv = rfh->GetView()) && rwhv->IsMouseLocked();
}

base::WeakPtr<AutofillPopupView>
AutofillPopupControllerImpl::CreateSubPopupView(
    base::WeakPtr<AutofillPopupController> controller) {
  return view_ ? view_->CreateSubPopupView(controller) : nullptr;
}

int AutofillPopupControllerImpl::GetPopupLevel() const {
  return !IsRootPopup() ? parent_controller_->get()->GetPopupLevel() + 1 : 0;
}

void AutofillPopupControllerImpl::FireControlsChangedEvent(bool is_show) {
  if (!accessibility_state_utils::IsScreenReaderEnabled())
    return;

  // Retrieve the ax tree id associated with the current web contents.
  ui::AXTreeID tree_id;
  if (content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame()) {
    tree_id = rfh->GetAXTreeID();
  }

  // Retrieve the ax node id associated with the current web contents' element
  // that has a controller relation to the current autofill popup.
  int32_t node_id = delegate_->GetWebContentsPopupControllerAxId();

  // In order to get the AXPlatformNode for the ax node id, we first need
  // the AXPlatformNode for the web contents.
  ui::AXPlatformNode* root_platform_node =
      GetRootAXPlatformNodeForWebContents();
  if (!root_platform_node)
    return;

  ui::AXPlatformNodeDelegate* root_platform_node_delegate =
      root_platform_node->GetDelegate();
  if (!root_platform_node_delegate)
    return;

  // Now get the target node from its tree ID and node ID.
  ui::AXPlatformNode* target_node =
      root_platform_node_delegate->GetFromTreeIDAndNodeID(tree_id, node_id);
  if (!target_node || !view_) {
    return;
  }

  absl::optional<int32_t> popup_ax_id = view_->GetAxUniqueId();
  if (!popup_ax_id) {
    return;
  }

  // All the conditions are valid, raise the accessibility event and set global
  // popup ax unique id.
  if (is_show) {
    ui::SetActivePopupAxUniqueId(popup_ax_id);
  } else {
    ui::ClearActivePopupAxUniqueId();
  }

  target_node->NotifyAccessibilityEvent(ax::mojom::Event::kControlsChanged);
}

ui::AXPlatformNode*
AutofillPopupControllerImpl::GetRootAXPlatformNodeForWebContents() {
  if (!web_contents()) {
    return nullptr;
  }

  auto* rwhv = web_contents()->GetRenderWidgetHostView();
  if (!rwhv)
    return nullptr;

  // RWHV gives us a NativeViewAccessible.
  gfx::NativeViewAccessible native_view_accessible =
      rwhv->GetNativeViewAccessible();
  if (!native_view_accessible)
    return nullptr;

  // NativeViewAccessible corresponds to an AXPlatformNode.
  return ui::AXPlatformNode::FromNativeViewAccessible(native_view_accessible);
}

}  // namespace autofill
