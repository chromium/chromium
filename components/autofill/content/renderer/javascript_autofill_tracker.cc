// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/javascript_autofill_tracker.h"

#include <algorithm>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"

namespace autofill {

namespace {

// The minimum number of fields that must be modified by JS during the tracking
// window to detect it as a custom JS autofill without enforcing additional
// conditions.
constexpr size_t kJsAutofillMinFieldsChanged = 3;

// The number of fields modified by JS during the tracking window after which it
// stops being detected as a custom JS autofill.
constexpr size_t kJsAutofillMaxFieldsChanged = 10;

// The maximum time gap between JS modifications to be considered part of the
// same JS autofill event.
constexpr base::TimeDelta kJsAutofillMaxTimeGap = base::Milliseconds(200);

// Returns true if `element` can act as the anchor for a custom JS-autofill
// popup. Custom autofill dropdowns are typically attached to text-like input
// fields (e.g., text, search, email, telephone, url) or textarea elements.
// Non-textual controls (e.g., checkboxes, radio buttons, select elements,
// date pickers) return false to avoid tracking unrelated user interactions.
bool IsPossibleAnchorElement(blink::WebFormControlElement element) {
  std::optional<mojom::FormControlType> field_type =
      form_util::GetAutofillFormControlType(element);
  if (!field_type) {
    return false;
  }
  switch (*field_type) {
    case mojom::FormControlType::kInputText:
    case mojom::FormControlType::kInputSearch:
    case mojom::FormControlType::kInputEmail:
    case mojom::FormControlType::kInputTelephone:
    case mojom::FormControlType::kInputUrl:
    case mojom::FormControlType::kTextArea:
      return true;
    case mojom::FormControlType::kContentEditable:
      // Contenteditable elements are not extracted by default in Autofill and
      // are excluded in this function for simplicity.
      return false;
    case mojom::FormControlType::kInputCheckbox:
    case mojom::FormControlType::kInputMonth:
    case mojom::FormControlType::kInputNumber:
    case mojom::FormControlType::kInputPassword:
    case mojom::FormControlType::kInputRadio:
    case mojom::FormControlType::kSelectOne:
    case mojom::FormControlType::kInputDate:
    case mojom::FormControlType::kInputHiddenEmailVerification:
      return false;
  }
  NOTREACHED();
}

// Returns true if `target_node` represents a plausible custom JS-autofill
// dropdown suggestion. Custom dropdown items are rendered using non-form DOM
// elements (e.g., <div>, <li>, <span>).This is simply a heuristic ruling out
// common cases and is not exhaustive.
bool IsPossibleMouseClickTargetElement(blink::WebNode target_node) {
  // If `target_node` is a non-element node (e.g. text inside a <button> or
  // <div>), walk up to its parent element to determine the actual DOM control
  // element.
  while (target_node && !target_node.IsElementNode()) {
    target_node = target_node.ParentNode();
  }
  // Custom JS dropdown options (e.g. <div>, <li>, <span>) are non-form
  // elements. Clicking any form control element (buttons, checkboxes, radio
  // buttons, etc.) should not start the custom JS autofill detection timer.
  return !target_node.DynamicTo<blink::WebFormControlElement>();
}

mojom::JavaScriptModificationType GetJavaScriptModificationType(
    std::u16string_view old_value,
    std::u16string_view new_value) {
  if (old_value == new_value) {
    return mojom::JavaScriptModificationType::kTrivial;
  }
  if (old_value.empty()) {
    return mojom::JavaScriptModificationType::kEmptyToNonEmpty;
  }
  if (new_value.empty()) {
    return mojom::JavaScriptModificationType::kClearing;
  }
  if (new_value.size() > old_value.size() &&
      base::StartsWith(new_value, old_value,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return mojom::JavaScriptModificationType::kPrefixCompletion;
  }
  return mojom::JavaScriptModificationType::kReassignment;
}

}  // namespace

JavaScriptAutofillTracker::JavaScriptAutofillTracker(
    blink::WebLocalFrame* web_frame,
    DidDetectCallback callback)
    : web_frame_(*web_frame), callback_(std::move(callback)) {
  CHECK(!callback_.is_null());
}

JavaScriptAutofillTracker::~JavaScriptAutofillTracker() {
  timer_.Stop();
}

void JavaScriptAutofillTracker::OnJavaScriptChangedValue(
    const blink::WebFormControlElement& element,
    const blink::WebString& old_value) {
  // In order to add a log record to `js_logs_`, the following conditions must
  // be satisfied:

  // (1) A mousedown event must have started the timer not earlier than
  // `kMaxTimeGap` milliseconds ago. This is to increase the likelihood of the
  // JS change being caused by the click itself.
  if (!timer_.IsRunning()) {
    return;
  }

  // (2) `js_logs_` must still have less than `kJsAutofillMaxFieldsChanged`
  // records. If more than that many fields are modified in such a small window
  // of time, it is likely not an autofill dropdown. This is also a performance
  // guard since string analysis is performed below.
  if (js_logs_.size() >= kJsAutofillMaxFieldsChanged) {
    return;
  }

  // (3) The element whose value was set by JS should be autofillable and
  // focusable (which is an approximation of "visible"). Other JS modifications
  // are not interesting from a JS-autofill dropdown perspective.
  if (!form_util::IsAutofillableElement(element) || !element.IsFocusable()) {
    return;
  }

  js_logs_.push_back(mojom::JavaScriptFieldModification::New(
      form_util::GetFieldRendererId(element),
      GetJavaScriptModificationType(old_value.Utf16(),
                                    element.Value().Utf16())));
}

void JavaScriptAutofillTracker::Reset() {
  js_logs_.clear();
  timer_.Stop();
}

void JavaScriptAutofillTracker::HandleMousedown(
    const blink::WebNode& target_node) {
  // In order to start recording logs to `js_logs_`, the following conditions
  // must be satisfied:

  // (1) The frame must have transient user activation. This excludes events
  // happening without user interaction.
  blink::WebDocument document = web_frame_->GetDocument();
  if (!document || !web_frame_->HasTransientUserActivation()) {
    return;
  }

  // (2) The target of the mousedown event should be a possible target element
  // (e.g. not a checkbox, radio button, button element, etc.). This is to
  // exclude common cases where the user clicks away from an input element, but
  // not on a dropdown option.
  if (!IsPossibleMouseClickTargetElement(target_node)) {
    return;
  }

  // (3) The frame should have a non-null focused element whose type can be that
  // of an anchor element. This element will be used as the trigger field of the
  // JS autofill operation.
  blink::WebFormControlElement focused_element =
      document.FocusedElement().DynamicTo<blink::WebFormControlElement>();
  if (!focused_element || !IsPossibleAnchorElement(focused_element)) {
    return;
  }

  // (4) The timer isn't already running. This is to rule out cases where mouse
  // clicks happen repeatedly and quickly and keep extending the tracking window
  // indefinitely.
  if (timer_.IsRunning()) {
    return;
  }

  // This should not be needed in theory, but if for some reason logs exist in
  // the list BEFORE starting a timer run, they should not be included in the
  // final analysis done by `DetectJavaScriptAutofill()`.
  js_logs_.clear();

  timer_.Start(
      FROM_HERE, kJsAutofillMaxTimeGap,
      base::BindOnce(&JavaScriptAutofillTracker::DetectJavaScriptAutofill,
                     // Safe because `timer_` is owned by `this`. Destructing
                     // it cancels the task.
                     base::Unretained(this),
                     form_util::GetFieldRendererId(focused_element)));
}

void JavaScriptAutofillTracker::DetectJavaScriptAutofill(
    FieldRendererId trigger_element_id) {
  std::vector<mojom::JavaScriptFieldModificationPtr> logs = std::move(js_logs_);
  js_logs_.clear();
  if (logs.empty()) {
    return;
  }

  blink::WebFormControlElement trigger_element =
      form_util::GetFormControlByRendererId(trigger_element_id);
  if (!trigger_element) {
    return;
  }

  if (!IsPossibleAnchorElement(trigger_element)) {
    return;
  }

  std::erase_if(
      logs, [target_form = trigger_element.GetOwningFormForAutofill()](
                const mojom::JavaScriptFieldModificationPtr& record) {
        blink::WebFormControlElement element =
            form_util::GetFormControlByRendererId(record->field_id);
        return !element || element.GetOwningFormForAutofill() != target_form;
      });

  std::vector<mojom::JavaScriptFieldModificationPtr> field_modifications;
  absl::flat_hash_set<FieldRendererId> seen_fields;

  // `logs` is moved here to ensure it is not used below, since
  // `field_modifications` should be used in what follows as it contains the
  // filtered list of logs.
  for (mojom::JavaScriptFieldModificationPtr& record : std::move(logs)) {
    if (seen_fields.insert(record->field_id).second) {
      field_modifications.push_back(std::move(record));
    }
  }

  if (field_modifications.empty() ||
      field_modifications.size() >= kJsAutofillMaxFieldsChanged) {
    return;
  }

  // If too few fields were modified by JavaScript, then an extra condition is
  // enforced, which is that at least one field should be prefix-completed, in
  // order to reduce IPC noise and false positives.
  if (field_modifications.size() < kJsAutofillMinFieldsChanged &&
      !std::ranges::contains(
          field_modifications,
          mojom::JavaScriptModificationType::kPrefixCompletion,
          &mojom::JavaScriptFieldModification::modification_type)) {
    return;
  }

  callback_.Run(trigger_element, std::move(field_modifications));
}

}  // namespace autofill
