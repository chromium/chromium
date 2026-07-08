// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/at_memory_suggestion_controller.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/common/autofill_util.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

base::WeakPtr<AtMemorySuggestionController>
AtMemorySuggestionController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

AtMemorySuggestionController::AtMemorySuggestionController(
    base::WeakPtr<AutofillSuggestionDelegate> delegate,
    content::WebContents* web_contents,
    PopupControllerCommon controller_common)
    : delegate_(delegate),
      web_contents_(web_contents->GetWeakPtr()),
      controller_common_(std::move(controller_common)) {}

AtMemorySuggestionController::~AtMemorySuggestionController() = default;

void AtMemorySuggestionController::Hide(SuggestionHidingReason reason) {
  // The bottom sheet search bar is focused, which triggers a focus change.
  // Ignore these hiding reasons to keep the bottom sheet visible while the
  // user enters their query. SuggestionHidingReason::kUserAborted is passed
  // to this method when the bottom sheet is closed (e.g. dismissed by the
  // user).
  if (reason == SuggestionHidingReason::kEndEditing ||
      reason == SuggestionHidingReason::kFocusChanged) {
    return;
  }
  if (delegate_) {
    delegate_->OnSuggestionsHidden(reason);
  }
  HideViewAndDie();
}

void AtMemorySuggestionController::ViewDestroyed() {
  Hide(SuggestionHidingReason::kViewDestroyed);
}

gfx::NativeView AtMemorySuggestionController::container_view() const {
  return web_contents_ ? web_contents_->GetNativeView() : gfx::NativeView();
}

content::WebContents* AtMemorySuggestionController::GetWebContents() const {
  return web_contents_.get();
}

const gfx::RectF& AtMemorySuggestionController::element_bounds() const {
  return controller_common_.element_bounds;
}

PopupAnchorType AtMemorySuggestionController::anchor_type() const {
  return controller_common_.anchor_type;
}

base::i18n::TextDirection
AtMemorySuggestionController::GetElementTextDirection() const {
  return controller_common_.text_direction;
}

void AtMemorySuggestionController::OnSuggestionsChanged() {
  NOTREACHED();
}

void AtMemorySuggestionController::AcceptSuggestion(
    int index,
    AutofillMetrics::SuggestionAcceptedMethod accept_method) {
  if (base::checked_cast<size_t>(index) >= suggestions_.size()) {
    return;
  }

  // Use a copy instead of a reference here. Under certain circumstances,
  // `DidAcceptSuggestion()` can call `Show()` or trigger focus changes
  // that overwrite `suggestions_` and invalidate the reference.
  Suggestion suggestion = suggestions_[index];
  if (delegate_) {
    delegate_->DidAcceptSuggestion(
        suggestion, AutofillSuggestionDelegate::SuggestionMetadata{
                        .multi_index = {static_cast<size_t>(index)}});
  }
}

bool AtMemorySuggestionController::RemoveSuggestion(
    int index,
    AutofillMetrics::SingleEntryRemovalMethod removal_method) {
  NOTREACHED();
}

int AtMemorySuggestionController::GetLineCount() const {
  return suggestions_.size();
}

const std::vector<Suggestion>& AtMemorySuggestionController::GetSuggestions()
    const {
  return suggestions_;
}

const Suggestion& AtMemorySuggestionController::GetSuggestionAt(int row) const {
  return suggestions_[row];
}

FillingProduct AtMemorySuggestionController::GetMainFillingProduct() const {
  return FillingProduct::kAtMemory;
}

void AtMemorySuggestionController::Show(
    UiSessionId session_id,
    std::vector<Suggestion> suggestions,
    AutofillSuggestionTriggerSource trigger_source,
    AutoselectFirstSuggestion autoselect_first_suggestion,
    AutofillSuggestionsIgnoreFocusLoss ignore_focus_loss) {
  ui_session_id_ = session_id;
  suggestions_ = std::move(suggestions);
  trigger_source_ = trigger_source;

  if (auto* client =
          ChromeAutofillClient::FromWebContents(web_contents_.get())) {
    client->ShowAtMemoryBottomSheet(suggestions_, delegate_);
  }
  if (delegate_) {
    delegate_->OnSuggestionsShown(suggestions_, std::nullopt);
  }
}

std::optional<AutofillSuggestionController::UiSessionId>
AtMemorySuggestionController::GetUiSessionId() const {
  return ui_session_id_;
}

void AtMemorySuggestionController::SetKeepPopupOpenForTesting(
    bool keep_popup_open_for_testing) {
  // No-op for now.
}

void AtMemorySuggestionController::UpdateDataListValues(
    base::span<const SelectOption> options) {
  NOTREACHED();
}

void AtMemorySuggestionController::HideViewAndDie() {
  if (auto* client =
          ChromeAutofillClient::FromWebContents(web_contents_.get())) {
    client->HideAtMemoryBottomSheet();
  }
  ui_session_id_ = std::nullopt;

  // Invalidates in particular ChromeAutofillClient's WeakPtr to `this`, which
  // prevents recursive calls triggered by hiding or destroying the view.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // If a deletion task has already been scheduled, avoid posting a duplicate
  // one (which can happen if hide/destruction events are triggered
  // recursively).
  if (self_deletion_weak_ptr_factory_.HasWeakPtrs()) {
    return;
  }

  // Delete `this` asynchronously so it doesn't happen while the controller
  // still has active methods on the call stack, avoiding use-after-free.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<AtMemorySuggestionController> weak_this) {
                       if (weak_this) {
                         delete weak_this.get();
                       }
                     },
                     self_deletion_weak_ptr_factory_.GetWeakPtr()));
}

bool AtMemorySuggestionController::MayRecycle(
    base::WeakPtr<AutofillSuggestionDelegate> delegate,
    content::WebContents* web_contents,
    AutofillSuggestionTriggerSource trigger_source) const {
  return delegate_.get() == delegate.get() &&
         container_view() == web_contents->GetNativeView() &&
         IsAtMemoryTriggerSource(trigger_source);
}

void AtMemorySuggestionController::Recycle(
    PopupControllerCommon controller_common,
    int32_t form_control_ax_id) {
  controller_common_ = std::move(controller_common);
}

}  // namespace autofill
