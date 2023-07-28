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
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"

namespace autofill {

constexpr char16_t kLabelSeparator = ' ';
constexpr size_t kMaxBulletCount = 8;

namespace {
std::u16string CreateLabel(const Suggestion& suggestion) {
  std::u16string password =
      suggestion.additional_label.substr(0, kMaxBulletCount);
  // The label contains the signon_realm or is empty. The additional_label can
  // never be empty since it must contain a password.
  if (suggestion.labels.empty() || suggestion.labels[0][0].value.empty())
    return password;

  // TODO(crbug.com/1313616): Re-consider whether using DCHECK is an appropriate
  // way to explicitly regulate what information should be populated for the
  // interface.
  DCHECK_EQ(suggestion.labels.size(), 1U);
  DCHECK_EQ(suggestion.labels[0].size(), 1U);
  return suggestion.labels[0][0].value + kLabelSeparator + password;
}

}  // namespace

AutofillKeyboardAccessoryAdapter::AutofillKeyboardAccessoryAdapter(
    base::WeakPtr<AutofillPopupController> controller)
    : controller_(controller) {}

AutofillKeyboardAccessoryAdapter::~AutofillKeyboardAccessoryAdapter() = default;

// AutofillPopupView implementation.

void AutofillKeyboardAccessoryAdapter::Show(
    AutoselectFirstSuggestion autoselect_first_suggestion) {
  DCHECK(view_) << "Show called before a View was set!";
  OnSuggestionsChanged();
}

void AutofillKeyboardAccessoryAdapter::Hide() {
  DCHECK(view_) << "Hide called before a View was set!";
  view_->Hide();
}

bool AutofillKeyboardAccessoryAdapter::HandleKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  return false;
}

void AutofillKeyboardAccessoryAdapter::OnSuggestionsChanged() {
  TRACE_EVENT0("passwords",
               "AutofillKeyboardAccessoryAdapter::OnSuggestionsChanged");
  DCHECK(controller_) << "Call OnSuggestionsChanged only from its owner!";
  DCHECK(view_) << "OnSuggestionsChanged called before a View was set!";

  labels_.clear();
  front_element_ = absl::nullopt;
  for (int i = 0; i < GetLineCount(); ++i) {
    const Suggestion& suggestion = controller_->GetSuggestionAt(i);
    if (suggestion.popup_item_id != PopupItemId::kClearForm) {
      labels_.push_back(CreateLabel(suggestion));
      continue;
    }
    DCHECK(!front_element_.has_value()) << "Additional front item at: " << i;
    front_element_ = absl::optional<int>(i);
    // If there is a special popup item, just reuse the previously used label.
    std::vector<std::vector<Suggestion::Text>> suggestion_labels =
        controller_->GetSuggestionLabelsAt(i);
    if (suggestion_labels.empty()) {
      labels_.emplace_back();
    } else {
      labels_.emplace_back(std::move(suggestion_labels[0][0].value));
    }
  }

  view_->Show();
}

void AutofillKeyboardAccessoryAdapter::AxAnnounce(const std::u16string& text) {
  DCHECK(view_) << "AxAnnounce called before a View was set!";
  view_->AxAnnounce(text);
}

absl::optional<int32_t> AutofillKeyboardAccessoryAdapter::GetAxUniqueId() {
  NOTIMPLEMENTED() << "See https://crbug.com/985927";
  return absl::nullopt;
}

base::WeakPtr<AutofillPopupView>
AutofillKeyboardAccessoryAdapter::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// AutofillPopupController implementation.

void AutofillKeyboardAccessoryAdapter::AcceptSuggestion(int index) {
  if (controller_) {
    controller_->AcceptSuggestion(OffsetIndexFor(index));
  }
}

void AutofillKeyboardAccessoryAdapter::AcceptSuggestionWithoutThreshold(
    int index) {
  if (controller_) {
    controller_->AcceptSuggestionWithoutThreshold(OffsetIndexFor(index));
  }
}

int AutofillKeyboardAccessoryAdapter::GetLineCount() const {
  return controller_ ? controller_->GetLineCount() : 0;
}

const autofill::Suggestion& AutofillKeyboardAccessoryAdapter::GetSuggestionAt(
    int row) const {
  DCHECK(controller_) << "Call GetSuggestionAt only from its owner!";
  return controller_->GetSuggestionAt(OffsetIndexFor(row));
}

std::u16string AutofillKeyboardAccessoryAdapter::GetSuggestionMainTextAt(
    int row) const {
  DCHECK(controller_) << "Call GetSuggestionMainTextAt only from its owner!";
  return controller_->GetSuggestionMainTextAt(OffsetIndexFor(row));
}

std::u16string AutofillKeyboardAccessoryAdapter::GetSuggestionMinorTextAt(
    int row) const {
  DCHECK(controller_) << "Call GetSuggestionMinorTextAt only from its owner!";
  return controller_->GetSuggestionMinorTextAt(OffsetIndexFor(row));
}

std::vector<std::vector<Suggestion::Text>>
AutofillKeyboardAccessoryAdapter::GetSuggestionLabelsAt(int row) const {
  DCHECK(controller_) << "Call GetSuggestionLabelAt only from its owner!";
  DCHECK(static_cast<size_t>(row) < labels_.size());
  return {{Suggestion::Text(labels_[OffsetIndexFor(row)])}};
}

PopupType AutofillKeyboardAccessoryAdapter::GetPopupType() const {
  DCHECK(controller_) << "Call GetPopupType only from its owner!";
  return controller_->GetPopupType();
}

bool AutofillKeyboardAccessoryAdapter::GetRemovalConfirmationText(
    int index,
    std::u16string* title,
    std::u16string* body) {
  return controller_ && controller_->GetRemovalConfirmationText(
                            OffsetIndexFor(index), title, body);
}

bool AutofillKeyboardAccessoryAdapter::RemoveSuggestion(int index) {
  DCHECK(view_) << "RemoveSuggestion called before a View was set!";
  std::u16string title, body;
  if (!GetRemovalConfirmationText(index, &title, &body))
    return false;

  view_->ConfirmDeletion(
      title, body,
      base::BindOnce(&AutofillKeyboardAccessoryAdapter::OnDeletionConfirmed,
                     weak_ptr_factory_.GetWeakPtr(), index));
  return true;
}

void AutofillKeyboardAccessoryAdapter::SelectSuggestion(
    absl::optional<size_t> index) {
  if (!controller_)
    return;
  controller_->SelectSuggestion(
      index ? absl::optional<size_t>(OffsetIndexFor(*index)) : absl::nullopt);
}

// AutofillPopupViewDelegate implementation

void AutofillKeyboardAccessoryAdapter::Hide(PopupHidingReason reason) {
  if (controller_)
    controller_->Hide(reason);
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
  DCHECK(controller_) << "Call OnSuggestionsChanged only from its owner!";
  return controller_->container_view();
}

content::WebContents* AutofillKeyboardAccessoryAdapter::GetWebContents() const {
  return controller_->GetWebContents();
}

const gfx::RectF& AutofillKeyboardAccessoryAdapter::element_bounds() const {
  DCHECK(controller_) << "Call OnSuggestionsChanged only from its owner!";
  return controller_->element_bounds();
}

base::i18n::TextDirection
AutofillKeyboardAccessoryAdapter::GetElementTextDirection() const {
  DCHECK(controller_);
  return controller_->GetElementTextDirection();
}

std::vector<Suggestion> AutofillKeyboardAccessoryAdapter::GetSuggestions()
    const {
  if (!controller_)
    return std::vector<Suggestion>();
  std::vector<Suggestion> suggestions = controller_->GetSuggestions();
  if (front_element_.has_value()) {
    std::rotate(suggestions.begin(),
                suggestions.begin() + front_element_.value(),
                suggestions.begin() + front_element_.value() + 1);
  }
  return suggestions;
}

void AutofillKeyboardAccessoryAdapter::OnDeletionConfirmed(int index) {
  if (controller_)
    controller_->RemoveSuggestion(OffsetIndexFor(index));
}

int AutofillKeyboardAccessoryAdapter::OffsetIndexFor(int element_index) const {
  if (!front_element_.has_value())
    return element_index;
  if (0 == element_index)
    return front_element_.value();
  return element_index - (element_index <= front_element_.value() ? 1 : 0);
}

}  // namespace autofill
