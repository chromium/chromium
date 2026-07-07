// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_JAVASCRIPT_AUTOFILL_TRACKER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_JAVASCRIPT_AUTOFILL_TRACKER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/blink/public/web/web_form_control_element.h"

namespace blink {
class WebLocalFrame;
}

namespace autofill {

// Tracks JavaScript-triggered value changes in form fields to detect potential
// custom JS-based autofill mechanisms like custom address pickers implemented
// by web pages.
class JavaScriptAutofillTracker {
 public:
  // Holds information about a single JS-triggered value change event.
  struct JsChangeRecord {
    // The ID of the field whose value was modified by JS.
    FieldRendererId modified_field_id;
    // The ID of the field that was focused when the modification occurred.
    FieldRendererId focused_field_id;
    // The time when the modification was recorded.
    base::TimeTicks timestamp;
  };

  // Callback signature invoked when a JS-autofill event is detected.
  using DidDetectCallback = base::RepeatingCallback<void(
      FormRendererId form_id,
      FieldRendererId trigger_field_id,
      const std::vector<FieldRendererId>& field_ids)>;

  JavaScriptAutofillTracker(blink::WebLocalFrame* web_frame,
                            DidDetectCallback callback);
  JavaScriptAutofillTracker(const JavaScriptAutofillTracker&) = delete;
  JavaScriptAutofillTracker& operator=(const JavaScriptAutofillTracker&) =
      delete;
  ~JavaScriptAutofillTracker();

  void OnJavaScriptChangedValue(const blink::WebFormControlElement& element);

  // Called when the browser is about to autofill a form (and not JavaScript).
  // This is used to allow the tracker to distinguish between browser and
  // JavaScript autofilling a form.
  void OnWillAutofillForm();

  // Clears all recorded changes and stops the detection timer.
  void Reset();

 private:
  friend class JavaScriptAutofillTrackerTestApi;

  // Analyzes the recorded changes in `js_logs_` to determine if they constitute
  // a JavaScript autofill event. If so, invokes `callback_`.
  void DetectJavaScriptAutofill();

  // The owning frame.
  const raw_ref<blink::WebLocalFrame> web_frame_;

  // Callback to notify the owner of a detected JS autofill.
  DidDetectCallback callback_;

  // A rolling log of recent JavaScript-triggered value changes.
  std::vector<JsChangeRecord> js_logs_;

  // Timer used to wait for a sequence of JS changes to complete before
  // analyzing them.
  base::OneShotTimer timer_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_JAVASCRIPT_AUTOFILL_TRACKER_H_
