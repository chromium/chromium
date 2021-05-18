// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_active_popup.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"

#if defined(OS_ANDROID)
#include "chrome/browser/autofill/manual_filling_controller_impl.h"

using FillingSource = ManualFillingController::FillingSource;
#endif

using base::WeakPtr;

namespace autofill {

#if !defined(OS_MAC)
// static
WeakPtr<AutofillPopupControllerImpl> AutofillPopupControllerImpl::GetOrCreate(
    WeakPtr<AutofillPopupControllerImpl> previous,
    WeakPtr<AutofillPopupDelegate> delegate,
    content::WebContents* web_contents,
    gfx::NativeView container_view,
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction) {
  if (previous.get() && previous->delegate_.get() == delegate.get() &&
      previous->container_view() == container_view) {
    previous->SetElementBounds(element_bounds);
    previous->ClearState();
    return previous;
  }

  if (previous.get())
    previous->Hide(PopupHidingReason::kViewDestroyed);

  AutofillPopupControllerImpl* controller = new AutofillPopupControllerImpl(
      delegate, web_contents, container_view, element_bounds, text_direction);
  return controller->GetWeakPtr();
}
#endif

AutofillPopupControllerImpl::AutofillPopupControllerImpl(
    base::WeakPtr<AutofillPopupDelegate> delegate,
    content::WebContents* web_contents,
    gfx::NativeView container_view,
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction)
    : controller_common_(element_bounds, text_direction, container_view),
      web_contents_(web_contents),
      delegate_(delegate) {
  ClearState();
  delegate->RegisterDeletionCallback(base::BindOnce(
      &AutofillPopupControllerImpl::HideViewAndDie, GetWeakPtr()));
}

AutofillPopupControllerImpl::~AutofillPopupControllerImpl() = default;

void AutofillPopupControllerImpl::Show(
    const std::vector<Suggestion>& suggestions,
    bool autoselect_first_suggestion,
    PopupType popup_type) {
  SetValues(suggestions);

  bool just_created = false;
  if (!view_) {
    view_ = AutofillPopupView::Create(GetWeakPtr());

    // It is possible to fail to create the popup, in this case
    // treat the popup as hiding right away.
    if (!view_) {
      delegate_->OnPopupSuppressed();
      Hide(PopupHidingReason::kViewDestroyed);
      return;
    }
    just_created = true;
  }

  WeakPtr<AutofillPopupControllerImpl> weak_this = GetWeakPtr();
  if (just_created) {
#if defined(OS_ANDROID)
    ManualFillingController::GetOrCreate(web_contents_)
        ->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                   !suggestions.empty());
#endif
    view_->Show();
    // crbug.com/1055981. |this| can be destroyed synchronously at this point.
    if (!weak_this)
      return;

    // We only fire the event when a new popup shows. We do not fire the
    // event when suggestions changed.
    FireControlsChangedEvent(true);

    if (autoselect_first_suggestion)
      SetSelectedLine(0);
  } else {
    if (selected_line_ && *selected_line_ >= GetLineCount())
      selected_line_.reset();

    OnSuggestionsChanged();
    // crbug.com/1200766. |this| can be destroyed synchronously at this point.
    if (!weak_this)
      return;
  }

  static_cast<ContentAutofillDriver*>(delegate_->GetAutofillDriver())
      ->RegisterKeyPressHandler(base::BindRepeating(
          [](base::WeakPtr<AutofillPopupControllerImpl> weak_this,
             const content::NativeWebKeyboardEvent& event) {
            return weak_this && weak_this->HandleKeyPressEvent(event);
          },
          GetWeakPtr()));

  delegate_->OnPopupShown();
}

void AutofillPopupControllerImpl::UpdateDataListValues(
    const std::vector<std::u16string>& values,
    const std::vector<std::u16string>& labels) {
  selected_line_.reset();
  // Remove all the old data list values, which should always be at the top of
  // the list if they are present.
  while (!suggestions_.empty() &&
         suggestions_[0].frontend_id == POPUP_ITEM_ID_DATALIST_ENTRY) {
    suggestions_.erase(suggestions_.begin());
  }

  // If there are no new data list values, exit (clearing the separator if there
  // is one).
  if (values.empty()) {
    if (!suggestions_.empty() &&
        suggestions_[0].frontend_id == POPUP_ITEM_ID_SEPARATOR) {
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
      suggestions_[0].frontend_id != POPUP_ITEM_ID_SEPARATOR) {
    suggestions_.insert(suggestions_.begin(), Suggestion());
    suggestions_[0].frontend_id = POPUP_ITEM_ID_SEPARATOR;
  }

  // Prepend the parameters to the suggestions we already have.
  suggestions_.insert(suggestions_.begin(), values.size(), Suggestion());
  for (size_t i = 0; i < values.size(); i++) {
    suggestions_[i].value = values[i];
    suggestions_[i].label = labels[i];
    suggestions_[i].frontend_id = POPUP_ITEM_ID_DATALIST_ENTRY;
  }

  OnSuggestionsChanged();
}

void AutofillPopupControllerImpl::PinView() {
  is_view_pinned_ = true;
}

base::span<const Suggestion>
AutofillPopupControllerImpl::GetUnelidedSuggestions() const {
  return base::span<const Suggestion>(suggestions_);
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
      reason == PopupHidingReason::kWidgetChanged)
    return;  // Don't close the popup because the browser window is resized.
  if (delegate_) {
    delegate_->ClearPreviewedForm();
    delegate_->OnPopupHidden();
    static_cast<ContentAutofillDriver*>(delegate_->GetAutofillDriver())
        ->RemoveKeyPressHandler();
  }

  HideViewAndDie();
}

void AutofillPopupControllerImpl::ViewDestroyed() {
  // The view has already been destroyed so clear the reference to it.
  view_ = nullptr;

  Hide(PopupHidingReason::kViewDestroyed);
}

bool AutofillPopupControllerImpl::HandleKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  switch (event.windows_key_code) {
    case ui::VKEY_UP:
      SelectPreviousLine();
      return true;
    case ui::VKEY_DOWN:
      SelectNextLine();
      return true;
    case ui::VKEY_PRIOR:  // Page up.
      // Set no line and then select the next line in case the first line is not
      // selectable.
      SetSelectedLine(base::nullopt);
      SelectNextLine();
      return true;
    case ui::VKEY_NEXT:  // Page down.
      SetSelectedLine(GetLineCount() - 1);
      return true;
    case ui::VKEY_ESCAPE:
      Hide(PopupHidingReason::kUserAborted);
      return true;
    case ui::VKEY_DELETE:
      return (event.GetModifiers() &
              content::NativeWebKeyboardEvent::kShiftKey) &&
             RemoveSelectedLine();
    case ui::VKEY_TAB:
      // A tab press should cause the selected line to be accepted, but still
      // return false so the tab key press propagates and changes the cursor
      // location.
      AcceptSelectedLine();
      return false;
    case ui::VKEY_RETURN:
      return AcceptSelectedLine();
    default:
      return false;
  }
}

void AutofillPopupControllerImpl::OnSuggestionsChanged() {
#if defined(OS_ANDROID)
  // Assume that suggestions are (still) available. If this is wrong, the method
  // |HideViewAndDie| will be called soon after and will hide all suggestions.
  ManualFillingController::GetOrCreate(web_contents_)
      ->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                 /*has_suggestions=*/true);
#endif

  // Platform-specific draw call.
  view_->OnSuggestionsChanged();
}

void AutofillPopupControllerImpl::SelectionCleared() {
  SetSelectedLine(base::nullopt);
}

void AutofillPopupControllerImpl::AcceptSuggestion(int index) {
  const Suggestion& suggestion = suggestions_[index];
#if defined(OS_ANDROID)
  auto mf_controller = ManualFillingController::GetOrCreate(web_contents_);
  // Accepting a suggestion should hide all suggestions. To prevent them from
  // coming up in Multi-Window mode, mark the source as unavailable.
  mf_controller->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                          /*has_suggestions=*/false);
  mf_controller->Hide();
#endif
  delegate_->DidAcceptSuggestion(suggestion.value, suggestion.frontend_id,
                                 index);
}

gfx::NativeView AutofillPopupControllerImpl::container_view() const {
  return controller_common_.container_view;
}

content::WebContents* AutofillPopupControllerImpl::GetWebContents() const {
  return web_contents_;
}

const gfx::RectF& AutofillPopupControllerImpl::element_bounds() const {
  return controller_common_.element_bounds;
}

void AutofillPopupControllerImpl::SetElementBounds(const gfx::RectF& bounds) {
  controller_common_.element_bounds.set_origin(bounds.origin());
  controller_common_.element_bounds.set_size(bounds.size());
}

bool AutofillPopupControllerImpl::IsRTL() const {
  return controller_common_.text_direction == base::i18n::RIGHT_TO_LEFT;
}

std::vector<Suggestion> AutofillPopupControllerImpl::GetSuggestions() const {
  return suggestions_;
}

int AutofillPopupControllerImpl::GetLineCount() const {
  return suggestions_.size();
}

const Suggestion& AutofillPopupControllerImpl::GetSuggestionAt(int row) const {
  return suggestions_[row];
}

const std::u16string& AutofillPopupControllerImpl::GetSuggestionValueAt(
    int row) const {
  return suggestions_[row].value;
}

const std::u16string& AutofillPopupControllerImpl::GetSuggestionLabelAt(
    int row) const {
  return suggestions_[row].label;
}

bool AutofillPopupControllerImpl::GetRemovalConfirmationText(
    int list_index,
    std::u16string* title,
    std::u16string* body) {
  return delegate_->GetDeletionConfirmationText(
      suggestions_[list_index].value, suggestions_[list_index].frontend_id,
      title, body);
}

bool AutofillPopupControllerImpl::RemoveSuggestion(int list_index) {
  // This function might be called in a callback, so ensure the list index is
  // still in bounds. If not, terminate the removing and consider it failed.
  // TODO(crbug.com/1209792): Replace these checks with a stronger identifier.
  if (list_index < 0 || static_cast<size_t>(list_index) >= suggestions_.size())
    return false;
  if (!delegate_->RemoveSuggestion(suggestions_[list_index].value,
                                   suggestions_[list_index].frontend_id)) {
    return false;
  }

  // Remove the deleted element.
  suggestions_.erase(suggestions_.begin() + list_index);

  selected_line_.reset();

  if (HasSuggestions()) {
    delegate_->ClearPreviewedForm();
    OnSuggestionsChanged();
  } else {
    Hide(PopupHidingReason::kNoSuggestions);
  }

  return true;
}

base::Optional<int> AutofillPopupControllerImpl::selected_line() const {
  return selected_line_;
}

PopupType AutofillPopupControllerImpl::GetPopupType() const {
  return delegate_->GetPopupType();
}

void AutofillPopupControllerImpl::SetSelectedLine(
    base::Optional<int> selected_line) {
  if (selected_line_ == selected_line)
    return;

  if (selected_line) {
    DCHECK_LT(*selected_line, GetLineCount());
    if (!CanAccept(suggestions_[*selected_line].frontend_id))
      selected_line = base::nullopt;
  }

  auto previous_selected_line(selected_line_);
  selected_line_ = selected_line;
  view_->OnSelectedRowChanged(previous_selected_line, selected_line_);

  if (selected_line_) {
    delegate_->DidSelectSuggestion(suggestions_[*selected_line_].value,
                                   suggestions_[*selected_line_].frontend_id);
  } else {
    delegate_->ClearPreviewedForm();
  }
}

void AutofillPopupControllerImpl::SelectNextLine() {
  int new_selected_line = selected_line_ ? *selected_line_ + 1 : 0;

  // Skip over any lines that can't be selected.
  while (new_selected_line < GetLineCount() &&
         !CanAccept(suggestions_[new_selected_line].frontend_id)) {
    ++new_selected_line;
  }

  if (new_selected_line >= GetLineCount())
    new_selected_line = 0;

  SetSelectedLine(new_selected_line);
}

void AutofillPopupControllerImpl::SelectPreviousLine() {
  int new_selected_line = selected_line_.value_or(0) - 1;

  // Skip over any lines that can't be selected.
  while (new_selected_line >= 0 &&
         !CanAccept(GetSuggestionAt(new_selected_line).frontend_id)) {
    --new_selected_line;
  }

  if (new_selected_line < 0)
    new_selected_line = GetLineCount() - 1;

  SetSelectedLine(new_selected_line);
}

bool AutofillPopupControllerImpl::RemoveSelectedLine() {
  if (!selected_line_)
    return false;

  DCHECK_LT(*selected_line_, GetLineCount());
  return RemoveSuggestion(*selected_line_);
}

bool AutofillPopupControllerImpl::CanAccept(int id) {
  return id != POPUP_ITEM_ID_SEPARATOR &&
         id != POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE &&
         id != POPUP_ITEM_ID_MIXED_FORM_MESSAGE && id != POPUP_ITEM_ID_TITLE;
}

bool AutofillPopupControllerImpl::HasSuggestions() {
  if (suggestions_.empty())
    return false;
  int id = suggestions_[0].frontend_id;
  return id > 0 || id == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY ||
         id == POPUP_ITEM_ID_PASSWORD_ENTRY ||
         id == POPUP_ITEM_ID_USERNAME_ENTRY ||
         id == POPUP_ITEM_ID_ACCOUNT_STORAGE_PASSWORD_ENTRY ||
         id == POPUP_ITEM_ID_ACCOUNT_STORAGE_USERNAME_ENTRY ||
         id == POPUP_ITEM_ID_DATALIST_ENTRY ||
         id == POPUP_ITEM_ID_SCAN_CREDIT_CARD;
}

void AutofillPopupControllerImpl::SetValues(
    const std::vector<Suggestion>& suggestions) {
  suggestions_ = suggestions;
}

WeakPtr<AutofillPopupControllerImpl> AutofillPopupControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool AutofillPopupControllerImpl::AcceptSelectedLine() {
  if (!selected_line_)
    return false;

  DCHECK_LT(*selected_line_, GetLineCount());

  if (!CanAccept(suggestions_[*selected_line_].frontend_id))
    return false;

  AcceptSuggestion(*selected_line_);
  return true;
}

void AutofillPopupControllerImpl::ClearState() {
  // Don't clear view_, because otherwise the popup will have to get regenerated
  // and this will cause flickering.
  suggestions_.clear();

  selected_line_.reset();
}

void AutofillPopupControllerImpl::HideViewAndDie() {
#if defined(OS_ANDROID)
  // Mark the popup-like filling sources as unavailable.
  // Note: We don't invoke ManualFillingController::Hide() here, as we might
  // switch between text input fields.
  ManualFillingController::GetOrCreate(web_contents_)
      ->UpdateSourceAvailability(FillingSource::AUTOFILL,
                                 /*has_suggestions=*/false);
#endif

  if (view_) {
    // We need to fire the event while view is not deleted yet.
    FireControlsChangedEvent(false);
    view_->Hide();
  }

  delete this;
}

void AutofillPopupControllerImpl::FireControlsChangedEvent(bool is_show) {
  if (!accessibility_state_utils::IsScreenReaderEnabled())
    return;
  DCHECK(view_);

  // Retrieve the ax tree id associated with the current web contents.
  ui::AXTreeID tree_id = delegate_->GetAutofillDriver()->GetAxTreeId();

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
  base::Optional<int32_t> popup_ax_id = view_->GetAxUniqueId();
  if (!target_node || !popup_ax_id)
    return;

  // All the conditions are valid, raise the accessibility event and set global
  // popup ax unique id.
  if (is_show)
    ui::SetActivePopupAxUniqueId(popup_ax_id);
  else
    ui::ClearActivePopupAxUniqueId();

  target_node->NotifyAccessibilityEvent(ax::mojom::Event::kControlsChanged);
}

ui::AXPlatformNode*
AutofillPopupControllerImpl::GetRootAXPlatformNodeForWebContents() {
  if (!web_contents_)
    return nullptr;

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

}  // namespace autofill
