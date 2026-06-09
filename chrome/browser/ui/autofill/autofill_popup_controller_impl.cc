// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/rtl.h"
#include "base/i18n/string_search.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_utils.h"
#include "chrome/browser/ui/autofill/next_idle_barrier.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "components/accessibility_annotator/core/accessibility_query_service.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/from_accessibility_annotator.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/ui/popup_interaction.h"
#include "components/autofill/core/browser/ui/tabbed_pane_enums.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/config.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_active_popup.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
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
        AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess};

// Trigger sources for which the `NextIdleBarrier` is not reset. Note that this
// requires that the trigger sources is only used to update the popup.
constexpr DenseSet<AutofillSuggestionTriggerSource>
    kTriggerSourcesExemptFromTimeReset = {
        AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess};

struct SuggestionFiltrationResult {
  void AddSuggestion(
      const Suggestion& suggestion,
      std::optional<AutofillPopupController::SuggestionFilterMatch>
          filter_match) {
    suggestions.push_back(suggestion);
    filter_matches.push_back(std::move(filter_match));
  }

  // Filtered suggestions, in display order.
  std::vector<Suggestion> suggestions;
  // Per-suggestion filter metadata aligned with `suggestions` by index.
  // `std::nullopt` means the suggestion matched but has no text range to
  // highlight.
  std::vector<std::optional<AutofillPopupController::SuggestionFilterMatch>>
      filter_matches;
};

SuggestionFiltrationResult FilterSuggestions(
    const std::vector<Suggestion>& suggestions,
    const AutofillPopupController::SuggestionFilter& filter) {
  SuggestionFiltrationResult result;
  result.suggestions.reserve(suggestions.size());
  result.filter_matches.reserve(suggestions.size());

  auto add_suggestion_filtration_result =
      [&result](const Suggestion& suggestion,
                std::optional<AutofillPopupController::SuggestionFilterMatch>
                    filter_match = std::nullopt) {
        result.AddSuggestion(suggestion, std::move(filter_match));
      };

  std::optional<base::i18n::FixedPatternStringSearch> search;
  if (std::holds_alternative<AutofillPopupController::StringFilter>(filter)) {
    search.emplace(*std::get<AutofillPopupController::StringFilter>(filter),
                   /*case_sensitive=*/false);
  }

  for (const Suggestion& suggestion : suggestions) {
    if (suggestion.filtration_policy ==
        Suggestion::FiltrationPolicy::kPresentOnlyWithoutFilter) {
      continue;
    } else if (suggestion.filtration_policy ==
               Suggestion::FiltrationPolicy::kStatic) {
      add_suggestion_filtration_result(suggestion);
    } else if (search) {
      size_t match_index = 0;
      size_t match_length = 0;
      if (search->Search(suggestion.main_text.value, &match_index,
                         &match_length, /*forward_search=*/true)) {
        add_suggestion_filtration_result(
            suggestion, AutofillPopupController::SuggestionFilterMatch{
                            .main_text_match = gfx::Range(
                                match_index, match_index + match_length)});
      }
    } else if (std::holds_alternative<SuggestionTabIndex>(filter) &&
               std::get<SuggestionTabIndex>(filter) == suggestion.tab_index) {
      add_suggestion_filtration_result(suggestion);
    }
  }

  return result;
}

void MaybeRecordAddressDeletedMetric(content::WebContents* web_contents,
                                     const Suggestion& suggestion) {
  if (web_contents) {
    PersonalDataManager* pdm = PersonalDataManagerFactory::GetForBrowserContext(
        web_contents->GetBrowserContext());

    const auto* payload =
        std::get_if<Suggestion::AutofillProfilePayload>(&suggestion.payload);
    if (pdm && payload) {
      const AutofillProfile* profile =
          pdm->address_data_manager().GetProfileByGUID(payload->guid.value());
      if (profile) {
        AutofillMetrics::LogDeleteAddressProfileFromPopup(
            profile->record_type());
      }
    }
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
    int32_t form_control_ax_id,
    AutofillSuggestionTriggerSource trigger_source) {
  if (previous &&
      previous->MayRecycle(delegate, web_contents, trigger_source)) {
    previous->Recycle(std::move(controller_common), form_control_ax_id);
    return previous;
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

bool AutofillPopupControllerImpl::MayRecycle(
    base::WeakPtr<AutofillSuggestionDelegate> delegate,
    content::WebContents* web_contents,
    AutofillSuggestionTriggerSource trigger_source) const {
  return delegate_.get() == delegate.get() &&
         container_view() == web_contents->GetNativeView() &&
         GetSuggestionTriggerSource() == trigger_source;
}

void AutofillPopupControllerImpl::Recycle(
    PopupControllerCommon controller_common,
    int32_t form_control_ax_id) {
  if (self_deletion_weak_ptr_factory_.HasWeakPtrs()) {
    self_deletion_weak_ptr_factory_.InvalidateWeakPtrs();
  }
  controller_common_ = std::move(controller_common);
  form_control_ax_id_ = form_control_ax_id;
  ClearState();
}

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
    AutoselectFirstSuggestion autoselect_first_suggestion,
    AutofillSuggestionsIgnoreFocusLoss ignore_focus_loss) {
  ui_session_id_ = ui_session_id;
  ignore_focus_loss_ = ignore_focus_loss;
  trigger_source_ = trigger_source;
  if (IsAtMemoryTriggerSource(trigger_source_)) {
    suggestions_filling_product_ = FillingProduct::kAtMemory;
  } else if (!suggestions.empty() &&
             IsStandaloneSuggestionType(suggestions[0].type)) {
    suggestions_filling_product_ =
        GetFillingProductFromSuggestionType(suggestions[0].type);
  } else {
    suggestions_filling_product_ = FillingProduct::kNone;
  }

  if (suggestions.empty() && !IsAtMemoryTriggerSource(trigger_source_) &&
      base::FeatureList::IsEnabled(
          features::kAutofillAndroidKeyboardAccessoryDynamicPositioning)) {
    Hide(SuggestionHidingReason::kNoSuggestions);
    return;
  }

  if (!suggestions.empty() &&
      suggestions[0].type == SuggestionType::kDatalistEntry) {
    AutofillMetrics::LogDataListSuggestionsShown();
  }

  const bool should_ignore_focus_loss =
      *ignore_focus_loss_ || (view_ && view_->HasFocus());

  // Autofill popups should only be shown in focused windows because on Windows
  // the popup may overlap the focused window (see crbug.com/40056880).
  if (auto* rwhv = web_contents_->GetRenderWidgetHostView();
      (!rwhv || !rwhv->HasFocus()) && IsRootPopup() &&
      !should_ignore_focus_loss) {
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
  if (!should_ignore_focus_loss &&
      (!rfh || !delegate_ ||
       !IsAncestorOf(GetRenderFrameHost(*delegate_), rfh))) {
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

  should_ignore_mouse_observed_outside_item_bounds_check_ =
      kTriggerSourcesExemptFromPaintChecks.contains(trigger_source_);
  if (!kTriggerSourcesExemptFromTimeReset.contains(trigger_source_)) {
    barrier_for_accepting_.reset();
  }

  if (view_) {
    OnSuggestionsChanged();
  } else {
    bool has_parent = parent_controller_ && parent_controller_->get();
    auto tabbed_pane_config =
        controller_common_.show_tabbed_popup
            ? std::make_optional<AutofillPopupView::TabbedPaneConfig>(
                  std::vector<AutofillPopupView::TabbedPaneConfig::Tab>{
                      {TabbedPaneTabType::kPayNow,
                       l10n_util::GetStringUTF16(IDS_AUTOFILL_PAY_NOW)},
                      {TabbedPaneTabType::kPayLater,
                       l10n_util::GetStringUTF16(IDS_AUTOFILL_PAY_LATER)}})
            : std::nullopt;
    view_ = has_parent
                ? parent_controller_->get()->CreateSubPopupView(GetWeakPtr())
                : AutofillPopupView::Create(GetWeakPtr(),
                                            GetSearchBarConfig(trigger_source),
                                            std::move(tabbed_pane_config));

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
          FROM_HERE, config.saved_state_timeout,
          base::BindOnce(&AutofillSuggestionController::Hide, GetWeakPtr(),
                         SuggestionHidingReason::kFadeTimerExpired));
    }

    delegate_->OnSuggestionsShown(non_filtered_suggestions_);
  }

  if (autofill_metrics::ShouldLogAutofillSuggestionShown(trigger_source_)) {
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

bool AutofillPopupControllerImpl::IsViewVisibilityAcceptingThresholdEnabled()
    const {
  return !disable_threshold_for_testing_;
}

bool AutofillPopupControllerImpl::IsSearching() const {
  return delegate_ && delegate_->IsSearching();
}

void AutofillPopupControllerImpl::Hide(SuggestionHidingReason reason) {
  const bool ignore_focus_loss =
      *ignore_focus_loss_ || (view_ && view_->HasFocus());
  // The end editing signal is sent when the currently focused field in the
  // renderer loses focus.
  if (ignore_focus_loss && (reason == SuggestionHidingReason::kFocusChanged ||
                            reason == SuggestionHidingReason::kEndEditing)) {
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
    delegate_->OnSuggestionsHidden(reason);
  }
  key_press_observer_.Reset();
  popup_hide_helper_.reset();
  // TODO(crbug.com/341916065): Consider only emitting this metric if the popup
  // has been opened before. Today the show method can call `Hide()` before
  // properly opening the popup.
  AutofillMetrics::LogAutofillSuggestionHidingReason(
      suggestions_filling_product_, reason);

  HideViewAndDie();
}

bool AutofillPopupControllerImpl::HasCreditCardSuggestions() const {
  return delegate_ &&
         delegate_->GetMainFillingProduct() == FillingProduct::kCreditCard;
}

void AutofillPopupControllerImpl::ViewDestroyed() {
  // The view has already been destroyed so clear the reference to it.
  view_ = nullptr;
  Hide(SuggestionHidingReason::kViewDestroyed);
}

void AutofillPopupControllerImpl::OnSuggestionsChanged() {
  OnSuggestionsChanged(
      controller_common_.prefer_prev_arrow_side_on_suggestions_update);
}

void AutofillPopupControllerImpl::AcceptSuggestion(
    int index,
    AutofillMetrics::SuggestionAcceptedMethod accept_method) {
  CHECK_LT(base::checked_cast<size_t>(index), GetSuggestions().size());
  CHECK(IsAcceptableSuggestionType(GetSuggestions()[index].type));

  // Ignore clicks immediately after the popup was shown. This is to prevent
  // users accidentally accepting suggestions (crbug.com/40058217).
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
  if (!suggestion.IsAcceptable()) {
    return;
  }

  NotifyUserEducationAboutAcceptedSuggestion(web_contents_.get(), suggestion);
  if (suggestion.acceptance_a11y_announcement && view_) {
    view_->AxAnnounce(*suggestion.acceptance_a11y_announcement);
  }

  AutofillMetrics::LogPopupInteraction(suggestions_filling_product_,
                                       GetPopupLevel(),
                                       PopupInteraction::kSuggestionAccepted);
  base::UmaHistogramEnumeration("Autofill.SuggestionAccepted.Method",
                                accept_method);
  delegate_->DidAcceptSuggestion(suggestion,
                                 AutofillSuggestionDelegate::SuggestionMetadata{
                                     .row = index,
                                     .sub_popup_level = GetPopupLevel(),
                                     .from_search_result = !!filter_});
}

gfx::NativeView AutofillPopupControllerImpl::container_view() const {
  return web_contents_ ? web_contents_->GetNativeView() : gfx::NativeView();
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

std::optional<AutofillPopupView::SearchBarConfig>
AutofillPopupControllerImpl::GetSearchBarConfig(
    AutofillSuggestionTriggerSource trigger_source) const {
  switch (trigger_source) {
    case AutofillSuggestionTriggerSource::kAtMemory:
    case AutofillSuggestionTriggerSource::kAtMemoryContextMenu:
      return AutofillPopupView::SearchBarConfig{
          .placeholder = l10n_util::GetStringUTF16(
              IDS_AUTOFILL_AT_MEMORY_POPUP_SEARCH_BAR_PLACEHOLDER),
          .no_results_message = u""};
    case AutofillSuggestionTriggerSource::kManualFallbackPasswords:
      return AutofillPopupView::SearchBarConfig{
          .placeholder = l10n_util::GetStringUTF16(
              IDS_AUTOFILL_POPUP_SEARCH_BAR_PASSWORDS_INPUT_PLACEHOLDER),
          .no_results_message = l10n_util::GetStringUTF16(
              IDS_AUTOFILL_POPUP_SEARCH_BAR_PASSWORDS_NOT_FOUND)};
    case AutofillSuggestionTriggerSource::kFormControlElementClicked:
    case AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick:
    case AutofillSuggestionTriggerSource::kContentEditableClicked:
    case AutofillSuggestionTriggerSource::kTextFieldValueChanged:
    case AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown:
    case AutofillSuggestionTriggerSource::kOpenTextDataListChooser:
    case AutofillSuggestionTriggerSource::kPasswordManager:
    case AutofillSuggestionTriggerSource::kiOS:
    case AutofillSuggestionTriggerSource::kComposeDialogLostFocus:
    case AutofillSuggestionTriggerSource::kComposeDelayedProactiveNudge:
    case AutofillSuggestionTriggerSource::kPasswordManagerProcessedFocusedField:
    case AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess:
    case AutofillSuggestionTriggerSource::kProactivePasswordRecovery:
    case AutofillSuggestionTriggerSource::kGlic:
    case AutofillSuggestionTriggerSource::kUnspecified:
    case AutofillSuggestionTriggerSource::kAtMemoryInactivityNudge:
      return std::nullopt;
  }
  NOTREACHED();
}

void AutofillPopupControllerImpl::UpdateFilteredSuggestions() {
  if (filter_) {
    SuggestionFiltrationResult filtration_result =
        FilterSuggestions(non_filtered_suggestions_, *filter_);
    filtered_suggestions_ = std::move(filtration_result.suggestions);
    suggestion_filter_matches_ = std::move(filtration_result.filter_matches);
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
            kKeyboardShiftDeletePressed: {
          MaybeRecordAddressDeletedMetric(web_contents_.get(),
                                          GetSuggestions()[list_index]);
          break;
        }
        case AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory:
          NOTREACHED();
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
      // TODO(crbug.com/41482065): Add metrics for credit cards.
      break;
    case FillingProduct::kNone:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kIban:
    case FillingProduct::kLoyaltyCard:
    case FillingProduct::kPasskey:
    case FillingProduct::kPassword:
    case FillingProduct::kCompose:
    case FillingProduct::kAutofillAi:
    case FillingProduct::kIdentityCredential:
    case FillingProduct::kDataList:
    case FillingProduct::kOneTimePassword:
    case FillingProduct::kAtMemory:
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

AutofillSuggestionTriggerSource
AutofillPopupControllerImpl::GetSuggestionTriggerSource() const {
  return trigger_source_;
}

bool AutofillPopupControllerImpl::HasSuggestions() const {
  return std::ranges::any_of(GetSuggestions(), &IsStandaloneSuggestionType,
                             &Suggestion::type);
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
  // (crbug.com/40204318).
  weak_ptr_factory_.InvalidateWeakPtrs();

  // TODO(crbug.com/40230669, crbug.com/40207703): Move this into the
  // asynchronous call?
  if (view_) {
    // We need to fire the event while view is not deleted yet.
    FireControlsChangedEvent(false);
    view_->Hide();
    view_ = nullptr;
  }

  if (self_deletion_weak_ptr_factory_.HasWeakPtrs()) {
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<AutofillPopupControllerImpl> weak_this) {
                       if (weak_this) {
                         delete weak_this.get();
                       }
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
  if (!content::BrowserAccessibilityState::GetInstance()
           ->GetAccessibilityMode()
           .has_mode(ui::AXMode::kExtendedProperties)) {
    // Skip this additional tree processing unless the overall AXMode for the
    // browser process contains the flag for extended properties.
    return;
  }

  // In order to get the AXPlatformNode for the ax node id, we first need
  // the AXPlatformNode for the web contents.
  ui::AXPlatformNode* root_platform_node =
      GetRootAXPlatformNodeForWebContents();
  if (!root_platform_node) {
    return;
  }

  // Retrieve the ax tree id associated with the current web contents.
  ui::AXPlatformNodeDelegate* root_platform_node_delegate =
      root_platform_node->GetDelegate();
  ui::AXTreeID tree_id =
      root_platform_node_delegate->GetTreeData().focused_tree_id;

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
  if (!rwhv) {
    return nullptr;
  }

  // RWHV gives us a NativeViewAccessible.
  gfx::NativeViewAccessible native_view_accessible =
      rwhv->GetNativeViewAccessible();
  if (!native_view_accessible) {
    return nullptr;
  }

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
  if (!suggestion.IsAcceptable()) {
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
                   autoselect_first_suggestion,
                   AutofillSuggestionsIgnoreFocusLoss(false));
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
         !IsRootPopup();
}

void AutofillPopupControllerImpl::PerformButtonActionForSuggestion(
    int index,
    const SuggestionButtonAction& button_action) {
  CHECK_LE(base::checked_cast<size_t>(index), GetSuggestions().size());
  delegate_->DidPerformButtonActionForSuggestion(GetSuggestions()[index],
                                                 button_action);
}

const std::vector<
    std::optional<AutofillPopupController::SuggestionFilterMatch>>&
AutofillPopupControllerImpl::GetSuggestionFilterMatches() const {
  return suggestion_filter_matches_;
}

void AutofillPopupControllerImpl::SetFilter(
    std::optional<SuggestionFilter> filter,
    FilterSource source) {
  filter_ = std::move(filter);

  auto maybe_handle_with_delegate = [&]() {
    if (!delegate_) {
      return false;
    }

    std::u16string filter_value;
    if (filter_) {
      if (const auto* string_filter =
              std::get_if<AutofillPopupController::StringFilter>(&*filter_)) {
        filter_value = **string_filter;
      } else {
        return false;
      }
    }

    switch (source) {
      case FilterSource::kInputChanged:
        return delegate_->OnFilterChanged(filter_value);
      case FilterSource::kSearchSubmitted:
        return delegate_->OnSearchSubmitted(filter_value);
      case FilterSource::kTabSelected:
        return false;
    }
  };

  if (maybe_handle_with_delegate()) {
    return;
  }

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

void AutofillPopupControllerImpl::OnTabSelected(
    int tab_index,
    TabbedPaneTabType tabbed_pane_tab_type) {
  SetFilter(SuggestionTabIndex(tab_index), FilterSource::kTabSelected);
  if (delegate_) {
    delegate_->OnTabSelected(tabbed_pane_tab_type);
  }
}

bool AutofillPopupControllerImpl::HasFilteredOutSuggestions() const {
  return filter_.has_value() &&
         filtered_suggestions_.size() != non_filtered_suggestions_.size();
}

bool AutofillPopupControllerImpl::ShouldShowNoSuggestionsMessage() const {
  // If there is no filter, we should never show the "no results" message.
  if (!filter_.has_value()) {
    return false;
  }

  // If the search bar is configured to not show a "no results" message,
  // we should not show it.
  std::optional<AutofillPopupView::SearchBarConfig> search_bar_config =
      GetSearchBarConfig(trigger_source_);
  if (search_bar_config && search_bar_config->no_results_message.empty()) {
    return false;
  }

  // For other products, the popup is considered effectively empty if all
  // "filterable" suggestions (the ones that actually contain data to fill)
  // have been hidden by the filter.
  const bool has_any_filterable_suggestions = std::ranges::any_of(
      GetSuggestions(),
      [](Suggestion::FiltrationPolicy policy) {
        return policy != Suggestion::FiltrationPolicy::kStatic;
      },
      &Suggestion::filtration_policy);

  if (has_any_filterable_suggestions) {
    return false;
  }

  // We only show the message if the current filter actually hid anything
  // from the initial list. This prevents showing "No results" for a popup
  // that only contains footers from the start.
  return HasFilteredOutSuggestions();
}

}  // namespace autofill
