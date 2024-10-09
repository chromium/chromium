// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_utils.h"
#include "chrome/browser/ui/autofill/next_idle_barrier.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/ui/popup_interaction.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/config.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_active_popup.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace autofill {

namespace {

// Trigger sources for which no paint checks are enforced on the popup row
// level.
constexpr DenseSet<AutofillSuggestionTriggerSource>
    kTriggerSourcesExemptFromPaintChecks = {
        AutofillSuggestionTriggerSource::kManualFallbackAddress,
        AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess};

// Trigger sources for which the `NextIdleBarrier` is not reset. Note that this
// requires that the trigger sources is only used to update the popup.
constexpr DenseSet<AutofillSuggestionTriggerSource>
    kTriggerSourcesExemptFromTimeReset = {
        AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess};

using SuggestionFiltrationResult =
    std::pair<std::vector<Suggestion>,
              std::vector<AutofillPopupController::SuggestionFilterMatch>>;
SuggestionFiltrationResult FilterSuggestions(
    const std::vector<Suggestion>& suggestions,
    const AutofillPopupController::SuggestionFilter& filter) {
  SuggestionFiltrationResult result;

  std::u16string filter_lowercased = base::i18n::ToLower(*filter);
  for (size_t i = 0; i < suggestions.size(); ++i) {
    const Suggestion& suggestion = suggestions[i];
    if (suggestion.filtration_policy ==
        Suggestion::FiltrationPolicy::kPresentOnlyWithoutFilter) {
      continue;
    } else if (suggestion.filtration_policy ==
               Suggestion::FiltrationPolicy::kStatic) {
      result.first.push_back(suggestion);
      result.second.emplace_back();
    } else if (size_t pos = base::i18n::ToLower(suggestion.main_text.value)
                                .find(filter_lowercased);
               pos != std::u16string::npos) {
      result.first.push_back(suggestion);
      result.second.push_back(AutofillPopupController::SuggestionFilterMatch{
          .main_text_match = {pos, pos + filter->size()}});
    }
  }

  return result;
}

// Returns whether the controller should log the popup interaction shown metric.
// Some popups can be displayed without a direct user action (i.e. typing into a
// field or unfocusing a text are with a previous compose suggestion). We do not
// want to log popup shown interaction logs for them since they defeat the
// purpose of the metric.
bool ShouldLogPopupInteractionShown(
    AutofillSuggestionTriggerSource trigger_source) {
  switch (trigger_source) {
    case AutofillSuggestionTriggerSource::kUnspecified:
    case AutofillSuggestionTriggerSource::kFormControlElementClicked:
    case AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick:
    case AutofillSuggestionTriggerSource::kContentEditableClicked:
    case AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown:
    case AutofillSuggestionTriggerSource::kOpenTextDataListChooser:
    case AutofillSuggestionTriggerSource::kComposeDialogLostFocus:
    case AutofillSuggestionTriggerSource::kShowCardsFromAccount:
    case AutofillSuggestionTriggerSource::kPasswordManager:
    case AutofillSuggestionTriggerSource::kiOS:
    case AutofillSuggestionTriggerSource::
        kShowPromptAfterDialogClosedNonManualFallback:
    case AutofillSuggestionTriggerSource::kPasswordManagerProcessedFocusedField:
    case AutofillSuggestionTriggerSource::kManualFallbackAddress:
    case AutofillSuggestionTriggerSource::kManualFallbackPayments:
    case AutofillSuggestionTriggerSource::kManualFallbackPasswords:
    case AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses:
      return true;
    case AutofillSuggestionTriggerSource::kTextFieldDidChange:
    case AutofillSuggestionTriggerSource::kComposeDelayedProactiveNudge:
    case AutofillSuggestionTriggerSource::kPredictionImprovements:
    case AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess:
      return false;
  }
}

}  // namespace

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
// static
base::WeakPtr<AutofillSuggestionController>
AutofillSuggestionController::GetOrCreate(
    base::WeakPtr<AutofillSuggestionController> previous,
    base::WeakPtr<AutofillSuggestionDelegate> delegate,
    content::WebContents* web_contents,
    PopupControllerCommon controller_common,
    int32_t form_control_ax_id) {
  // All controllers on Desktop derive from `AutofillPopupControllerImpl`.
  if (AutofillPopupControllerImpl* previous_impl =
          static_cast<AutofillPopupControllerImpl*>(previous.get());
      previous_impl && previous_impl->delegate_.get() == delegate.get() &&
      previous_impl->container_view() == controller_common.container_view) {
    if (previous_impl->self_deletion_weak_ptr_factory_.HasWeakPtrs()) {
      previous_impl->self_deletion_weak_ptr_factory_.InvalidateWeakPtrs();
    }
    previous_impl->controller_common_ = std::move(controller_common);
    previous_impl->form_control_ax_id_ = form_control_ax_id;
    previous_impl->ClearState();
    return previous_impl->GetWeakPtr();
  }

  if (previous) {
    previous->Hide(SuggestionHidingReason::kViewDestroyed);
  }
  auto* controller = new AutofillPopupControllerImpl(
      delegate, web_contents, std::move(controller_common), form_control_ax_id,
      /*parent=*/std::nullopt);
  return controller->GetWeakPtr();
}
#endif

AutofillPopupControllerImpl::AutofillPopupControllerImpl(
    base::WeakPtr<AutofillSuggestionDelegate> delegate,
    content::WebContents* web_contents,
    PopupControllerCommon controller_common,
    int32_t form_control_ax_id,
    std::optional<base::WeakPtr<ExpandablePopupParentControllerImpl>> parent)
    : web_contents_(web_contents->GetWeakPtr()),
      controller_common_(std::move(controller_common)),
      delegate_(delegate),
      parent_controller_(parent) {
  ClearState();
}

AutofillPopupControllerImpl::~AutofillPopupControllerImpl() = default;

void AutofillPopupControllerImpl::Show(
    UiSessionId ui_session_id,
    std::vector<Suggestion> suggestions,
    AutofillSuggestionTriggerSource trigger_source,
    AutoselectFirstSuggestion autoselect_first_suggestion) {
  ui_session_id_ = ui_session_id;
  suggestions_filling_product_ =
      !suggestions.empty() && IsStandaloneSuggestionType(suggestions[0].type)
          ? GetFillingProductFromSuggestionType(suggestions[0].type)
          : FillingProduct::kNone;
  // Autofill popups should only be shown in focused windows because on Windows
  // the popup may overlap the focused window (see crbug.com/1239760).
  if (auto* rwhv = web_contents_->GetRenderWidgetHostView();
      (!rwhv || !rwhv->HasFocus()) && IsRootPopup()) {
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
      &AutofillPopupControllerImpl::Hide, base::Unretained(this));
  AutofillPopupHideHelper::PictureInPictureDetectionCallback
      pip_detection_callback = base::BindRepeating(
          [](base::WeakPtr<AutofillPopupControllerImpl> controller) {
            return controller && controller->view_ &&
                   controller->view_->OverlapsWithPictureInPictureWindow();
          },
          weak_ptr_factory_.GetWeakPtr());
  popup_hide_helper_.emplace(
      web_contents_.get(), rfh->GetGlobalId(), std::move(hiding_params),
      std::move(hiding_callback), std::move(pip_detection_callback));

  SetSuggestions(std::move(suggestions));

  trigger_source_ = trigger_source;
  should_ignore_mouse_observed_outside_item_bounds_check_ =
      kTriggerSourcesExemptFromPaintChecks.contains(trigger_source_);
  if (!kTriggerSourcesExemptFromTimeReset.contains(trigger_source_)) {
    barrier_for_accepting_.reset();
  }

  if (view_) {
    OnSuggestionsChanged();
  } else {
    bool has_parent = parent_controller_ && parent_controller_->get();
    auto search_bar_config =
        trigger_source_ ==
                AutofillSuggestionTriggerSource::kManualFallbackPasswords
            ? std::optional<AutofillPopupView::SearchBarConfig>(
                  {.placeholder = l10n_util::GetStringUTF16(
                       IDS_AUTOFILL_POPUP_SEARCH_BAR_PASSWORDS_INPUT_PLACEHOLDER),
                   .no_results_message = l10n_util::GetStringUTF16(
                       IDS_AUTOFILL_POPUP_SEARCH_BAR_PASSWORDS_NOT_FOUND)})
            : std::nullopt;
    view_ = has_parent
                ? parent_controller_->get()->CreateSubPopupView(GetWeakPtr())
                : AutofillPopupView::Create(GetWeakPtr(),
                                            std::move(search_bar_config));

    // It is possible to fail to create the popup, in this case
    // treat the popup as hiding right away.
    if (!view_) {
      Hide(SuggestionHidingReason::kViewDestroyed);
      return;
    }
    if (!view_->Show(autoselect_first_suggestion)) {
      return;
    }

    // We only fire the event when a new popup shows. We do not fire the
    // event when suggestions changed.
    FireControlsChangedEvent(true);
  }

  if (IsRootPopup()) {
    shown_time_ = base::TimeTicks::Now();

    // We may already be observing from a previous `Show` call.
    // TODO(crbug.com/41486228): Consider not to recycle views or controllers
    // and only permit a single call to `Show`.
    key_press_observer_.Reset();
    key_press_observer_.Observe(web_contents_->GetFocusedFrame());

    if (non_filtered_suggestions_.size() == 1 &&
        non_filtered_suggestions_[0].type ==
            SuggestionType::kComposeSavedStateNotification) {
      const compose::Config& config = compose::GetComposeConfig();
      fading_popup_timer_.Start(
          FROM_HERE,
          base::Milliseconds(config.saved_state_timeout_milliseconds),
          base::BindOnce(&AutofillSuggestionController::Hide, GetWeakPtr(),
                         SuggestionHidingReason::kFadeTimerExpired));
    }

    delegate_->OnSuggestionsShown(non_filtered_suggestions_);
  }

  if (ShouldLogPopupInteractionShown(trigger_source_)) {
    AutofillMetrics::LogPopupInteraction(suggestions_filling_product_,
                                         GetPopupLevel(),
                                         PopupInteraction::kPopupShown);
  }
}

std::optional<AutofillSuggestionController::UiSessionId>
AutofillPopupControllerImpl::GetUiSessionId() const {
  return view_ ? std::make_optional(ui_session_id_) : std::nullopt;
}

void AutofillPopupControllerImpl::SetKeepPopupOpenForTesting(
    bool keep_popup_open_for_testing) {
  keep_popup_open_for_testing_ = keep_popup_open_for_testing;
}

void AutofillPopupControllerImpl::UpdateDataListValues(
    base::span<const SelectOption> options) {
  non_filtered_suggestions_ = UpdateSuggestionsFromDataList(
      options, std::move(non_filtered_suggestions_));
  UpdateFilteredSuggestions();
  if (HasSuggestions()) {
    OnSuggestionsChanged();
  } else {
    Hide(SuggestionHidingReason::kNoSuggestions);
  }
}

void AutofillPopupControllerImpl::PinView() {
  is_view_pinned_ = true;
}

bool AutofillPopupControllerImpl::IsViewVisibilityAcceptingThresholdEnabled()
    const {
  return !disable_threshold_for_testing_;
}

void AutofillPopupControllerImpl::Hide(SuggestionHidingReason reason) {
  // If the reason for hiding is only stale data or a user interacting with
  // native Chrome UI (kFocusChanged/kEndEditing), the popup might be kept open.
  if (is_view_pinned_ && (reason == SuggestionHidingReason::kStaleData ||
                          reason == SuggestionHidingReason::kFocusChanged ||
                          reason == SuggestionHidingReason::kEndEditing)) {
    return;  // Don't close the popup while waiting for an update.
  }

  if ((reason == SuggestionHidingReason::kFocusChanged ||
       reason == SuggestionHidingReason::kEndEditing) &&
      view_ && view_->HasFocus()) {
    return;
  }

  // For tests, keep open when hiding is due to external stimuli.
  if (keep_popup_open_for_testing_ &&
      (reason == SuggestionHidingReason::kWidgetChanged ||
       reason == SuggestionHidingReason::kEndEditing)) {
    return;  // Don't close the popup because the browser window is resized or
             // because too many fields get focus one after each other (this can
             // happen on Desktop, if multiple password forms are present, and
             // they are all autofilled by default).
  }

  if (delegate_ && IsRootPopup()) {
    delegate_->ClearPreviewedForm();
    delegate_->OnSuggestionsHidden();
  }
  key_press_observer_.Reset();
  popup_hide_helper_.reset();
  // TODO(crbug.com/341916065): Consider only emitting this metric if the popup
  // has been opened before. Today the show method can call `Hide()` before
  // properly opening the popup.
  AutofillMetrics::LogAutofillSuggestionHidingReason(
      suggestions_filling_product_, reason);

  if (IsRootPopup() && shown_time_) {
    AutofillMetrics::LogAutofillPopupVisibleDuration(
        suggestions_filling_product_, base::TimeTicks::Now() - *shown_time_);
    shown_time_.reset();
  }

  HideViewAndDie();
}

void AutofillPopupControllerImpl::ViewDestroyed() {
  // The view has already been destroyed so clear the reference to it.
  view_ = nullptr;
  Hide(SuggestionHidingReason::kViewDestroyed);
}

void AutofillPopupControllerImpl::OnSuggestionsChanged() {
  OnSuggestionsChanged(/*prefer_prev_arrow_side=*/false);
}

void AutofillPopupControllerImpl::AcceptSuggestion(int index) {
  CHECK_LT(base::checked_cast<size_t>(index), GetSuggestions().size());
  CHECK(IsAcceptableSuggestionType(GetSuggestions()[index].type));

  // Ignore clicks immediately after the popup was shown. This is to prevent
  // users accidentally accepting suggestions (crbug.com/1279268).
  if ((!barrier_for_accepting_ || !barrier_for_accepting_->value()) &&
      !disable_threshold_for_testing_) {
    return;
  }

  if (IsPointerLocked(web_contents_.get())) {
    Hide(SuggestionHidingReason::kMouseLocked);
    return;
  }

  // Use a copy instead of a reference here. Under certain circumstances,
  // `DidAcceptSuggestion()` can call `SetSuggestions()` and invalidate the
  // reference.
  Suggestion suggestion = GetSuggestions()[index];
  if (!suggestion.is_acceptable) {
    return;
  }
  NotifyUserEducationAboutAcceptedSuggestion(web_contents_.get(), suggestion);
  if (suggestion.acceptance_a11y_announcement && view_) {
    view_->AxAnnounce(*suggestion.acceptance_a11y_announcement);
  }

  AutofillMetrics::LogPopupInteraction(suggestions_filling_product_,
                                       GetPopupLevel(),
                                       PopupInteraction::kSuggestionAccepted);
  delegate_->DidAcceptSuggestion(suggestion,
                                 AutofillSuggestionDelegate::SuggestionMetadata{
                                     .row = index,
                                     .sub_popup_level = GetPopupLevel(),
                                     .from_search_result = !!filter_});
}

gfx::NativeView AutofillPopupControllerImpl::container_view() const {
  return controller_common_.container_view;
}

content::WebContents* AutofillPopupControllerImpl::GetWebContents() const {
  return web_contents_.get();
}

const gfx::RectF& AutofillPopupControllerImpl::element_bounds() const {
  return controller_common_.element_bounds;
}

PopupAnchorType AutofillPopupControllerImpl::anchor_type() const {
  return controller_common_.anchor_type;
}

base::i18n::TextDirection AutofillPopupControllerImpl::GetElementTextDirection()
    const {
  return controller_common_.text_direction;
}

bool AutofillPopupControllerImpl::IsRootPopup() const {
  return !parent_controller_;
}

const std::vector<Suggestion>& AutofillPopupControllerImpl::GetSuggestions()
    const {
  return filter_ ? filtered_suggestions_ : non_filtered_suggestions_;
}

void AutofillPopupControllerImpl::OnSuggestionsChanged(
    bool prefer_prev_arrow_side) {
  if (view_) {
    view_->OnSuggestionsChanged(prefer_prev_arrow_side);
  }
}

void AutofillPopupControllerImpl::UpdateFilteredSuggestions() {
  if (filter_) {
    SuggestionFiltrationResult filtration_result =
        FilterSuggestions(non_filtered_suggestions_, *filter_);
    filtered_suggestions_ = std::move(filtration_result.first);
    suggestion_filter_matches_ = std::move(filtration_result.second);
  } else {
    filtered_suggestions_.clear();
    suggestion_filter_matches_.clear();
  }
}

int AutofillPopupControllerImpl::GetLineCount() const {
  return GetSuggestions().size();
}

const Suggestion& AutofillPopupControllerImpl::GetSuggestionAt(int row) const {
  return GetSuggestions()[row];
}

bool AutofillPopupControllerImpl::RemoveSuggestion(
    int list_index,
    AutofillMetrics::SingleEntryRemovalMethod removal_method) {
  if (IsPointerLocked(web_contents_.get())) {
    Hide(SuggestionHidingReason::kMouseLocked);
    return false;
  }

  // This function might be called in a callback, so ensure the list index is
  // still in bounds. If not, terminate the removing and consider it failed.
  // TODO(crbug.com/40766704): Replace these checks with a stronger identifier.
  if (list_index < 0 ||
      static_cast<size_t>(list_index) >= GetSuggestions().size()) {
    return false;
  }

  // Only first level suggestions can be deleted.
  if (GetPopupLevel() > 0) {
    return false;
  }

  if (!delegate_->RemoveSuggestion(GetSuggestions()[list_index])) {
    return false;
  }
  SuggestionType suggestion_type = GetSuggestions()[list_index].type;
  switch (GetFillingProductFromSuggestionType(suggestion_type)) {
    case FillingProduct::kAddress:
      switch (removal_method) {
        case AutofillMetrics::SingleEntryRemovalMethod::
            kKeyboardShiftDeletePressed:
          AutofillMetrics::LogDeleteAddressProfileFromPopup();
          break;
        case AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory:
          AutofillMetrics::LogDeleteAddressProfileFromKeyboardAccessory();
          break;
        case AutofillMetrics::SingleEntryRemovalMethod::kDeleteButtonClicked:
          NOTREACHED();
      }
      break;
    case FillingProduct::kAutocomplete:
      AutofillMetrics::OnAutocompleteSuggestionDeleted(removal_method);
      if (view_) {
        view_->AxAnnounce(l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_AUTOCOMPLETE_ENTRY_DELETED_A11Y_HINT,
            GetSuggestions()[list_index].main_text.value));
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
  if (filter_) {
    auto suggestion_iter = std::find(non_filtered_suggestions_.begin(),
                                     non_filtered_suggestions_.end(),
                                     GetSuggestions()[list_index]);
    CHECK(suggestion_iter != non_filtered_suggestions_.end());
    non_filtered_suggestions_.erase(suggestion_iter);
    UpdateFilteredSuggestions();
  } else {
    non_filtered_suggestions_.erase(non_filtered_suggestions_.begin() +
                                    list_index);
  }

  if (HasSuggestions()) {
    delegate_->ClearPreviewedForm();
    should_ignore_mouse_observed_outside_item_bounds_check_ =
        suggestion_type == SuggestionType::kAutocompleteEntry;
    OnSuggestionsChanged();
  } else {
    Hide(SuggestionHidingReason::kNoSuggestions);
  }

  return true;
}

FillingProduct AutofillPopupControllerImpl::GetMainFillingProduct() const {
  return delegate_->GetMainFillingProduct();
}

std::optional<AutofillClient::PopupScreenLocation>
AutofillPopupControllerImpl::GetPopupScreenLocation() const {
  return view_ ? view_->GetPopupScreenLocation()
               : std::make_optional<AutofillClient::PopupScreenLocation>();
}

bool AutofillPopupControllerImpl::HasSuggestions() const {
  return !GetSuggestions().empty() &&
         IsStandaloneSuggestionType(GetSuggestions()[0].type);
}

void AutofillPopupControllerImpl::SetSuggestions(
    std::vector<Suggestion> suggestions) {
  non_filtered_suggestions_ = std::move(suggestions);
  UpdateFilteredSuggestions();
}

base::WeakPtr<AutofillPopupController>
AutofillPopupControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillPopupControllerImpl::ClearState() {
  // Don't clear view_, because otherwise the popup will have to get
  // regenerated and this will cause flickering.
  filtered_suggestions_.clear();
  non_filtered_suggestions_.clear();
  any_suggestion_selected_ = false;
}

void AutofillPopupControllerImpl::HideViewAndDie() {
  HideSubPopup();

  // Invalidates in particular ChromeAutofillClient's WeakPtr to |this|, which
  // prevents recursive calls triggered by `view_->Hide()`
  // (crbug.com/1267047).
  weak_ptr_factory_.InvalidateWeakPtrs();

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
                     [](base::WeakPtr<AutofillPopupControllerImpl> weak_this) {
                       if (weak_this)
                         delete weak_this.get();
                     },
                     self_deletion_weak_ptr_factory_.GetWeakPtr()));
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
  if (content::RenderFrameHost* rfh = web_contents_->GetFocusedFrame()) {
    tree_id = rfh->GetAXTreeID();
  }

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
      root_platform_node_delegate->GetFromTreeIDAndNodeID(tree_id,
                                                          form_control_ax_id_);
  if (!target_node || !view_) {
    return;
  }

  std::optional<int32_t> popup_ax_id = view_->GetAxUniqueId();
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
  if (!web_contents_) {
    return nullptr;
  }

  auto* rwhv = web_contents_->GetRenderWidgetHostView();
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

AutofillPopupControllerImpl::KeyPressObserver::KeyPressObserver(
    AutofillPopupControllerImpl* observer)
    : observer_(CHECK_DEREF(observer)) {}

AutofillPopupControllerImpl::KeyPressObserver::~KeyPressObserver() {
  Reset();
}

void AutofillPopupControllerImpl::KeyPressObserver::Observe(
    content::RenderFrameHost* rfh) {
  rfh_ = rfh->GetGlobalId();
  handler_ = base::BindRepeating(
      // Cannot bind HandleKeyPressEvent() directly because of its
      // return value.
      [](base::WeakPtr<AutofillPopupControllerImpl> weak_this,
         const input::NativeWebKeyboardEvent& event) {
        return weak_this && weak_this->HandleKeyPressEvent(event);
      },
      observer_->weak_ptr_factory_.GetWeakPtr());
  rfh->GetRenderWidgetHost()->AddKeyPressEventCallback(handler_);
}

void AutofillPopupControllerImpl::KeyPressObserver::Reset() {
  if (auto* rfh = content::RenderFrameHost::FromID(rfh_)) {
    rfh->GetRenderWidgetHost()->RemoveKeyPressEventCallback(handler_);
  }
  rfh_ = {};
  handler_ = content::RenderWidgetHost::KeyPressEventCallback();
}

// AutofillPopupController implementation.

void AutofillPopupControllerImpl::SelectSuggestion(int index) {
  CHECK_LT(base::checked_cast<size_t>(index), GetSuggestions().size());
  CHECK(IsAcceptableSuggestionType(GetSuggestions()[index].type));

  if (IsPointerLocked(web_contents_.get())) {
    Hide(SuggestionHidingReason::kMouseLocked);
    return;
  }

  const autofill::Suggestion& suggestion = GetSuggestionAt(index);
  if (!suggestion.is_acceptable) {
    UnselectSuggestion();
    return;
  }

  if (!any_suggestion_selected_) {
    // Suggestion selection can happen multiple times for the same popup.
    // However we only emit it once to keep this metrics close to having a
    // funnel behaviour. Meaning the number of popups being shown is larger than
    // the number of popups being selected, which is larger than the number of
    // popups being accepted.
    AutofillMetrics::LogPopupInteraction(suggestions_filling_product_,
                                         GetPopupLevel(),
                                         PopupInteraction::kSuggestionSelected);
  }

  any_suggestion_selected_ = true;
  delegate_->DidSelectSuggestion(GetSuggestionAt(index));
}

void AutofillPopupControllerImpl::UnselectSuggestion() {
  delegate_->ClearPreviewedForm();
}

base::WeakPtr<AutofillSuggestionController>
AutofillPopupControllerImpl::OpenSubPopup(
    const gfx::RectF& anchor_bounds,
    std::vector<Suggestion> suggestions,
    AutoselectFirstSuggestion autoselect_first_suggestion) {
  PopupControllerCommon new_controller_common = controller_common_;
  new_controller_common.element_bounds = anchor_bounds;
  AutofillPopupControllerImpl* controller = new AutofillPopupControllerImpl(
      delegate_, web_contents_.get(), std::move(new_controller_common),
      /*form_control_ax_id=*/form_control_ax_id_,
      /*parent=*/weak_ptr_factory_.GetWeakPtr());

  // Show() can fail and cause controller deletion. Therefore store the weak
  // pointer before, so that this method returns null when that happens.
  sub_popup_controller_ = controller->weak_ptr_factory_.GetWeakPtr();
  controller->Show(ui_session_id_, std::move(suggestions), trigger_source_,
                   autoselect_first_suggestion);
  return sub_popup_controller_;
}

void AutofillPopupControllerImpl::HideSubPopup() {
  if (sub_popup_controller_) {
    sub_popup_controller_->Hide(
        SuggestionHidingReason::kExpandedSuggestionCollapsedSubPopup);
    sub_popup_controller_ = nullptr;
  }
}

bool AutofillPopupControllerImpl::
    ShouldIgnoreMouseObservedOutsideItemBoundsCheck() const {
  return should_ignore_mouse_observed_outside_item_bounds_check_ ||
         !IsRootPopup() ||
         base::FeatureList::IsEnabled(
             features::kAutofillPopupDisablePaintChecks);
}

void AutofillPopupControllerImpl::PerformButtonActionForSuggestion(
    int index,
    const SuggestionButtonAction& button_action) {
  CHECK_LE(base::checked_cast<size_t>(index), GetSuggestions().size());
  delegate_->DidPerformButtonActionForSuggestion(GetSuggestions()[index],
                                                 button_action);
}

const std::vector<AutofillPopupController::SuggestionFilterMatch>&
AutofillPopupControllerImpl::GetSuggestionFilterMatches() const {
  return suggestion_filter_matches_;
}

void AutofillPopupControllerImpl::SetFilter(
    std::optional<SuggestionFilter> filter) {
  filter_ = std::move(filter);
  UpdateFilteredSuggestions();
  OnSuggestionsChanged(/*prefer_prev_arrow_side=*/true);
}

bool AutofillPopupControllerImpl::HandleKeyPressEvent(
    const input::NativeWebKeyboardEvent& event) {
  if (sub_popup_controller_ &&
      sub_popup_controller_->HandleKeyPressEvent(event)) {
    return true;
  }

  return view_ && view_->HandleKeyPressEvent(event);
}

void AutofillPopupControllerImpl::OnPopupPainted() {
  if (!barrier_for_accepting_) {
    barrier_for_accepting_ = NextIdleBarrier::CreateNextIdleBarrierWithDelay(
        kIgnoreEarlyClicksOnSuggestionsDuration);
  }
}

bool AutofillPopupControllerImpl::HasFilteredOutSuggestions() const {
  return filter_.has_value() &&
         filtered_suggestions_.size() != non_filtered_suggestions_.size();
}

}  // namespace autofill
