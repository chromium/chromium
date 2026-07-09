// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/javascript_autofill_tracker.h"

#include <algorithm>

#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace autofill {

namespace {
// The min/max number of fields that must be modified by JS to be considered
// as a custom JS autofill.
constexpr size_t kJsAutofillMinFieldsChanged = 3;
constexpr size_t kJsAutofillMaxFieldsChanged = 10;

// The maximum time gap between JS modifications to be considered part of the
// same JS autofill event.
constexpr base::TimeDelta kJsAutofillMaxTimeGap = base::Milliseconds(200);

// The maximum number of logs we store before evicting the oldest.
constexpr size_t kMaxStoredJsLogs = 100;
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
    const blink::WebFormControlElement& element) {
  // In order to add a log record to `js_logs_`, the following conditions must
  // be satisfied:

  // (1) The frame must have transient user activation.
  blink::WebDocument document = web_frame_->GetDocument();
  if (!document || !web_frame_->HasTransientUserActivation()) {
    return;
  }

  // (2) The frame should have a non-null and autofillable focused element.
  blink::WebFormControlElement focused_element =
      document.FocusedElement().DynamicTo<blink::WebFormControlElement>();
  if (!focused_element || !form_util::IsAutofillableElement(focused_element)) {
    return;
  }

  // (3) The element whose value was set by JS should also be autofillable.
  if (!form_util::IsAutofillableElement(element)) {
    return;
  }

  // (4) `js_logs_` must still have less than `kMaxStoredJsLogs` records.
  if (js_logs_.size() >= kMaxStoredJsLogs) {
    return;
  }

  js_logs_.push_back(JsChangeRecord{
      .modified_field_id = form_util::GetFieldRendererId(element),
      .focused_field_id = form_util::GetFieldRendererId(focused_element),
      .timestamp = base::TimeTicks::Now(),
  });

  if (!timer_.IsRunning()) {
    timer_.Start(
        FROM_HERE, kJsAutofillMaxTimeGap,
        base::BindOnce(&JavaScriptAutofillTracker::DetectJavaScriptAutofill,
                       // Safe because `timer_` is owned by `this`. Destructing
                       // it cancels the task.
                       base::Unretained(this)));
  }
}

void JavaScriptAutofillTracker::OnWillAutofillForm() {
  // It is very unlikely to happen, but it could still happen that this timer
  // start cancels a timer start done by `OnJavaScriptChangedValue()` if the
  // user somehow manages to trigger both JS autofill and browser autofill at
  // the same time.
  // This is however still better than only starting the timer if it is not
  // running, because otherwise refills (which are browser autofill operations
  // fired very shortly after a previous one) could miss the timer and trigger a
  // false positive.
  timer_.Start(
      FROM_HERE, kJsAutofillMaxTimeGap,
      base::BindOnce(
          // Autofill modifies multiple fields simultaneously, which can trigger
          // multiple focus/blur/valuechange events, possibly leading to
          // multiple JS modifications (formatting, clearing, etc.). This
          // ensures that `DetectJavaScriptAutofill()` is not fired and
          // `js_logs_` is not corrupted, avoiding false positives.
          [](JavaScriptAutofillTracker* tracker) { tracker->js_logs_.clear(); },
          // Safe because `timer_` is owned by `this`. Destructing it cancels
          // the task.
          base::Unretained(this)));
}

void JavaScriptAutofillTracker::Reset() {
  js_logs_.clear();
  timer_.Stop();
}

void JavaScriptAutofillTracker::DetectJavaScriptAutofill() {
  std::vector<JsChangeRecord> logs = std::move(js_logs_);
  js_logs_.clear();
  CHECK(!logs.empty());

  blink::WebFormControlElement first_focused_field =
      form_util::GetFormControlByRendererId(logs.front().focused_field_id);
  if (!first_focused_field) {
    return;
  }

  // A JS-autofill picker would usually be anchored on a text-like input or
  // textarea element. We exclude select elements to avoid false positives
  // from country/state dropdowns resetting other fields and checkboxes and
  // radio buttons to avoid false positives caused by checking boxes like
  // "Billing address is similar to shipping address".
  std::optional<mojom::FormControlType> field_type =
      form_util::GetAutofillFormControlType(first_focused_field);
  if (!field_type) {
    return;
  }
  switch (*field_type) {
    case mojom::FormControlType::kInputText:
    case mojom::FormControlType::kInputSearch:
    case mojom::FormControlType::kInputEmail:
    case mojom::FormControlType::kInputTelephone:
    case mojom::FormControlType::kInputUrl:
    case mojom::FormControlType::kTextArea:
      break;
    case mojom::FormControlType::kContentEditable:
    case mojom::FormControlType::kInputCheckbox:
    case mojom::FormControlType::kInputMonth:
    case mojom::FormControlType::kInputNumber:
    case mojom::FormControlType::kInputPassword:
    case mojom::FormControlType::kInputRadio:
    case mojom::FormControlType::kSelectOne:
    case mojom::FormControlType::kInputDate:
    case mojom::FormControlType::kInputHiddenEmailVerification:
      return;
  }

  blink::WebFormElement target_form =
      first_focused_field.GetOwningFormForAutofill();
  std::erase_if(logs, [&](const JsChangeRecord& record) {
    blink::WebFormControlElement element =
        form_util::GetFormControlByRendererId(record.modified_field_id);
    return !element || element.GetOwningFormForAutofill() != target_form;
  });

  base::flat_set<FieldRendererId> unique_fields =
      base::MakeFlatSet<FieldRendererId>(logs, /*comp=*/{},
                                         &JsChangeRecord::modified_field_id);

  if (unique_fields.size() < kJsAutofillMinFieldsChanged ||
      unique_fields.size() > kJsAutofillMaxFieldsChanged) {
    return;
  }

  callback_.Run(first_focused_field, base::ToVector(unique_fields));
}

}  // namespace autofill
