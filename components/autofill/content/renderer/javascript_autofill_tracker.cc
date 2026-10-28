// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/javascript_autofill_tracker.h"

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"

namespace autofill {

namespace {

// The number of fields modified by JS during the tracking window after which it
// stops being detected as a custom JS autofill.
constexpr size_t kJsAutofillMaxFieldsChanged = 20;

// The maximum time gap between JS modifications to be considered part of the
// same JS autofill event.
constexpr base::TimeDelta kJsAutofillMaxTimeGap = base::Milliseconds(1000);

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
    case mojom::FormControlType::kInputMonth:
    case mojom::FormControlType::kInputNumber:
    case mojom::FormControlType::kInputPassword:
    case mojom::FormControlType::kSelectOne:
    case mojom::FormControlType::kInputDate:
      return false;
  }
  NOTREACHED();
}

// Returns true if `target_node` represents a plausible custom JS-autofill
// dropdown suggestion. Custom dropdown items are typically rendered using
// non-form DOM elements (e.g., <div>, <li>, <span>) or <button> elements with
// a relevant ARIA option/listbox/menu role on the button or an ancestor. This
// is simply a heuristic ruling out common cases and is not exhaustive.
bool IsPossibleMouseClickTargetElement(blink::WebNode target_node) {
  // If `target_node` is a non-element node (e.g. text inside a <button> or
  // <div>), walk up to its parent element to determine the actual DOM control
  // element.
  while (target_node && !target_node.IsElementNode()) {
    target_node = target_node.ParentNode();
  }
  // Clicking empty background space (e.g. <html>, <body>, or <form>) to blur a
  // field is never a dropdown option selection and should not start the
  // detection timer.
  auto target_element = target_node.DynamicTo<blink::WebElement>();
  if (!target_element || target_element.DynamicTo<blink::WebFormElement>() ||
      target_element.HasHTMLTagName("html") ||
      target_element.HasHTMLTagName("body")) {
    return false;
  }
  // Custom JS dropdown options (e.g. <div>, <li>, <span>) are generally
  // non-form elements. Clicking form control elements (inputs, checkboxes,
  // radio buttons, submit/reset buttons, etc.) should not start the custom JS
  // autofill detection timer.
  auto control_element =
      target_element.DynamicTo<blink::WebFormControlElement>();
  if (!control_element) {
    return true;
  }

  // Some autocomplete frameworks render dropdown suggestions as `<button>`
  // elements (either `<button type="button">` or a default `<button>` without a
  // `type` attribute, which defaults to `kButtonSubmit` in HTML) for built-in
  // click and accessibility semantics.
  const blink::mojom::FormControlType control_type =
      control_element.FormControlTypeForAutofill();
  const bool is_candidate_button =
      control_type == blink::mojom::FormControlType::kButtonButton ||
      (control_type == blink::mojom::FormControlType::kButtonSubmit &&
       !base::EqualsCaseInsensitiveASCII(
           control_element.GetAttribute("type").Utf16(), u"submit"));
  if (!is_candidate_button) {
    return false;
  }

  // Allow such buttons only when they or an ancestor have an ARIA
  // option/listbox/menu role, while still rejecting general form buttons (e.g.
  // reset buttons or explicit `type="submit"` buttons).
  for (blink::WebNode node = control_element; node; node = node.ParentNode()) {
    if (auto element = node.DynamicTo<blink::WebElement>()) {
      std::u16string role = element.GetAttribute("role").Utf16();
      if (base::EqualsCaseInsensitiveASCII(role, u"option") ||
          base::EqualsCaseInsensitiveASCII(role, u"listbox") ||
          base::EqualsCaseInsensitiveASCII(role, u"menu") ||
          base::EqualsCaseInsensitiveASCII(role, u"menuitem")) {
        return true;
      }
    }
  }
  return false;
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
  if (new_value.size() > old_value.size()) {
    std::u16string lower_old = base::i18n::ToLower(old_value);
    std::u16string lower_new = base::i18n::ToLower(new_value);
    if (lower_new.starts_with(lower_old)) {
      return mojom::JavaScriptModificationType::kPrefixCompletion;
    }
    constexpr size_t kMinSubstringMatchSize = 3u;
    if (lower_old.size() >= kMinSubstringMatchSize &&
        lower_new.contains(lower_old)) {
      return mojom::JavaScriptModificationType::kSubstringCompletion;
    }
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

void JavaScriptAutofillTracker::Reset() {
  js_logs_.clear();
  timer_.Stop();
}

void JavaScriptAutofillTracker::HandlePointerdown(
    const blink::WebNode& target_node) {
  // In order to start recording logs to `js_logs_`, the following conditions
  // must be satisfied:

  // (1) The frame must have transient user activation. This excludes events
  // happening without user interaction.
  blink::WebDocument document = web_frame_->GetDocument();
  if (!document || !web_frame_->HasTransientUserActivation()) {
    return;
  }

  // (2) The target of the pointerdown event should be a possible target element
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
                     form_util::GetFieldRendererId(focused_element),
                     base::TimeTicks::Now()));
}

void JavaScriptAutofillTracker::OnJavaScriptSetValue(
    const blink::WebFormControlElement& element,
    const blink::WebString& old_value) {
  // In order to add a log record to `js_logs_`, the following conditions must
  // be satisfied:

  // (1) A pointerdown event must have started the timer not earlier than
  // `kMaxTimeGap` milliseconds ago. This is to increase the likelihood of the
  // JS change being caused by the click itself.
  if (!timer_.IsRunning()) {
    return;
  }

  // (2) `js_logs_` must still have less than `kJsAutofillMaxFieldsChanged`
  // records. If more than that many fields are modified in such a small window
  // of time, it is likely not an autofill dropdown.
  if (js_logs_.size() >= kJsAutofillMaxFieldsChanged) {
    return;
  }

  // (3) The element whose value was set by JS should be autofillable. Other JS
  // modifications are not interesting from a JS-autofill dropdown perspective.
  if (!form_util::IsAutofillableElement(element)) {
    return;
  }

  js_logs_.emplace_back(form_util::GetFieldRendererId(element),
                        old_value.Utf16(), base::TimeTicks::Now());
}

void JavaScriptAutofillTracker::DetectJavaScriptAutofill(
    FieldRendererId trigger_element_id,
    base::TimeTicks timer_start_timestamp) {
  std::vector<LogRecord> logs = std::move(js_logs_);
  js_logs_.clear();
  if (logs.empty()) {
    return;
  }

  // Discard the detection window if the anchor element disappeared or changed
  // substantially during the timer run.
  blink::WebFormControlElement trigger_element =
      form_util::GetFormControlByRendererId(trigger_element_id);
  if (!trigger_element || !IsPossibleAnchorElement(trigger_element)) {
    return;
  }

  const blink::WebFormElement target_form =
      trigger_element.GetOwningFormForAutofill();
  std::vector<mojom::JavaScriptFieldModificationPtr> field_modifications;
  absl::flat_hash_set<FieldRendererId> seen_fields;

  // Populate `field_modifications` with one entry per modified visible field in
  // `target_form`. Because JS frameworks may modify a field multiple times
  // during a single dropdown selection (e.g., clearing the value first and
  // subsequently setting it to the selected value), only the first `LogRecord`
  // per field is processed and its `previous_value` is compared against the
  // field's current (final) value.
  for (const LogRecord& record : logs) {
    // (1) Only keep the first record per field so that `record.previous_value`
    // holds the field's value prior to any JS modifications during the tracking
    // window.
    if (!seen_fields.insert(record.field_id).second) {
      continue;
    }

    // (2) Ignore fields that were removed from the DOM or are hidden (e.g.
    // hidden billing fields synced from shipping fields).
    blink::WebFormControlElement element =
        form_util::GetFormControlByRendererId(record.field_id);
    if (!element || !element.IsFocusable()) {
      continue;
    }

    // (3) For simplicity, only consider fields belonging to the same form as
    // the anchor element, since the browser receives a single form to analyze
    // for detecting dropdowns.
    if (element.GetOwningFormForAutofill() != target_form) {
      continue;
    }

    field_modifications.push_back(mojom::JavaScriptFieldModification::New(
        record.field_id,
        GetJavaScriptModificationType(record.previous_value,
                                      element.Value().Utf16()),
        record.timestamp));
  }

  if (field_modifications.empty() ||
      field_modifications.size() >= kJsAutofillMaxFieldsChanged) {
    return;
  }

  callback_.Run(trigger_element, std::move(field_modifications),
                timer_start_timestamp);
}

}  // namespace autofill
