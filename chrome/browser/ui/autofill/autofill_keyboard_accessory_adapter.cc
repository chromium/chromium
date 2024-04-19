// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_adapter.h"

#include <numeric>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_view.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/metrics/granular_filling_metrics.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"

namespace autofill {

AutofillKeyboardAccessoryAdapter::AutofillKeyboardAccessoryAdapter(
    base::WeakPtr<AutofillKeyboardAccessoryController> controller)
    : controller_(controller) {}

AutofillKeyboardAccessoryAdapter::~AutofillKeyboardAccessoryAdapter() = default;

// AutofillPopupView implementation.

bool AutofillKeyboardAccessoryAdapter::Show(
    AutoselectFirstSuggestion autoselect_first_suggestion) {
  CHECK(view_) << "Show called before a View was set!";
  view_->Show();
  return true;
}

void AutofillKeyboardAccessoryAdapter::Hide() {
  CHECK(view_) << "Hide called before a View was set!";
  view_->Hide();
}

bool AutofillKeyboardAccessoryAdapter::OverlapsWithPictureInPictureWindow()
    const {
  // TODO(crbug.com/40280362): Hide the KA suggestion if it overlaps with
  // picture-in-picture window.
  return false;
}

bool AutofillKeyboardAccessoryAdapter::HandleKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  NOTREACHED_NORETURN();
}

void AutofillKeyboardAccessoryAdapter::OnSuggestionsChanged() {
  view_->Show();
}

void AutofillKeyboardAccessoryAdapter::AxAnnounce(const std::u16string& text) {
  CHECK(view_) << "AxAnnounce called before a View was set!";
  view_->AxAnnounce(text);
}

std::optional<int32_t> AutofillKeyboardAccessoryAdapter::GetAxUniqueId() {
  NOTREACHED_NORETURN();
}

base::WeakPtr<AutofillPopupView>
AutofillKeyboardAccessoryAdapter::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<AutofillPopupView>
AutofillKeyboardAccessoryAdapter::CreateSubPopupView(
    base::WeakPtr<AutofillSuggestionController> controller) {
  NOTREACHED_NORETURN();
}

// AutofillSuggestionController implementation.

void AutofillKeyboardAccessoryAdapter::AcceptSuggestion(int index) {
  NOTREACHED_NORETURN();
}

int AutofillKeyboardAccessoryAdapter::GetLineCount() const {
  NOTREACHED_NORETURN();
}

const autofill::Suggestion& AutofillKeyboardAccessoryAdapter::GetSuggestionAt(
    int row) const {
  NOTREACHED_NORETURN();
}

std::vector<std::vector<Suggestion::Text>>
AutofillKeyboardAccessoryAdapter::GetSuggestionLabelsAt(int row) const {
  NOTREACHED_NORETURN();
}

FillingProduct AutofillKeyboardAccessoryAdapter::GetMainFillingProduct() const {
  NOTREACHED_NORETURN();
}

bool AutofillKeyboardAccessoryAdapter::GetRemovalConfirmationText(
    int index,
    std::u16string* title,
    std::u16string* body) {
  return controller_ &&
         controller_->GetRemovalConfirmationText(index, title, body);
}

bool AutofillKeyboardAccessoryAdapter::RemoveSuggestion(
    int index,
    AutofillMetrics::SingleEntryRemovalMethod removal_method) {
  CHECK_EQ(removal_method,
           AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory);
  CHECK(view_) << "RemoveSuggestion called before a View was set!";
  std::u16string title;
  std::u16string body;
  if (!controller_ ||
      !controller_->GetRemovalConfirmationText(index, &title, &body)) {
    return false;
  }

  view_->ConfirmDeletion(
      title, body,
      base::BindOnce(&AutofillKeyboardAccessoryAdapter::OnDeletionDialogClosed,
                     weak_ptr_factory_.GetWeakPtr(), index));
  return true;
}

void AutofillKeyboardAccessoryAdapter::SelectSuggestion(int index) {
  NOTREACHED_NORETURN();
}

void AutofillKeyboardAccessoryAdapter::UnselectSuggestion() {
  NOTREACHED_NORETURN();
}

void AutofillKeyboardAccessoryAdapter::Show(
    std::vector<Suggestion> suggestions,
    AutofillSuggestionTriggerSource trigger_source,
    AutoselectFirstSuggestion autoselect_first_suggestion) {
  NOTREACHED_NORETURN();
}

void AutofillKeyboardAccessoryAdapter::DisableThresholdForTesting(
    bool disable_threshold) {
  NOTREACHED_NORETURN();
}

void AutofillKeyboardAccessoryAdapter::KeepPopupOpenForTesting() {
  NOTREACHED_NORETURN();
}

void AutofillKeyboardAccessoryAdapter::SetViewForTesting(
    base::WeakPtr<AutofillPopupView> view) {
  NOTREACHED_NORETURN();
}

void AutofillKeyboardAccessoryAdapter::UpdateDataListValues(
    base::span<const SelectOption> options) {
  NOTREACHED_NORETURN();
}

void AutofillKeyboardAccessoryAdapter::PinView() {
  NOTREACHED_NORETURN();
}

// AutofillPopupViewDelegate implementation

void AutofillKeyboardAccessoryAdapter::Hide(PopupHidingReason reason) {
  NOTREACHED_NORETURN();
}

void AutofillKeyboardAccessoryAdapter::ViewDestroyed() {
  if (controller_)
    controller_->ViewDestroyed();

  view_.reset();

  // The controller has now deleted itself.
  controller_ = nullptr;
  delete this;  // Remove dangling weak reference.
}

gfx::NativeView AutofillKeyboardAccessoryAdapter::container_view() const {
  NOTREACHED_NORETURN();
}

content::WebContents* AutofillKeyboardAccessoryAdapter::GetWebContents() const {
  return controller_->GetWebContents();
}

const gfx::RectF& AutofillKeyboardAccessoryAdapter::element_bounds() const {
  CHECK(controller_) << "Call OnSuggestionsChanged only from its owner!";
  return controller_->element_bounds();
}

base::i18n::TextDirection
AutofillKeyboardAccessoryAdapter::GetElementTextDirection() const {
  CHECK(controller_);
  return controller_->GetElementTextDirection();
}

std::vector<Suggestion> AutofillKeyboardAccessoryAdapter::GetSuggestions()
    const {
  NOTREACHED_NORETURN();
}

std::optional<AutofillClient::PopupScreenLocation>
AutofillKeyboardAccessoryAdapter::GetPopupScreenLocation() const {
  NOTREACHED_NORETURN();
}

base::WeakPtr<AutofillKeyboardAccessoryController>
AutofillKeyboardAccessoryAdapter::GetWeakPtrToController() {
  NOTREACHED_NORETURN() << "The view should not call this method directly";
}

void AutofillKeyboardAccessoryAdapter::OnDeletionDialogClosed(int index,
                                                              bool confirmed) {
  if (confirmed) {
    if (controller_) {
      controller_->RemoveSuggestion(
          index, AutofillMetrics::SingleEntryRemovalMethod::kKeyboardAccessory);
    }
    return;
  }
  if (controller_ && GetFillingProductFromPopupItemId(
                         controller_->GetSuggestionAt(index).popup_item_id) ==
                         FillingProduct::kAddress) {
    autofill_metrics::LogDeleteAddressProfileFromExtendedMenu(
        /*user_accepted_delete=*/false);
  }
}

}  // namespace autofill
