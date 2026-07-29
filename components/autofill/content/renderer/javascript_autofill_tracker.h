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
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_form_control_element.h"

namespace blink {
class WebLocalFrame;
}  // namespace blink

namespace autofill {

// Tracks JavaScript-triggered value changes in form fields to detect potential
// custom JS-based autofill mechanisms like custom address pickers implemented
// by web pages.
class JavaScriptAutofillTracker {
 public:
  // Callback signature invoked when a JS-autofill event is detected.
  using DidDetectCallback = base::RepeatingCallback<void(
      blink::WebFormControlElement trigger_field,
      std::vector<mojom::JavaScriptFieldModificationPtr> field_modifications)>;

  JavaScriptAutofillTracker(blink::WebLocalFrame* web_frame,
                            DidDetectCallback callback);
  JavaScriptAutofillTracker(const JavaScriptAutofillTracker&) = delete;
  JavaScriptAutofillTracker& operator=(const JavaScriptAutofillTracker&) =
      delete;
  ~JavaScriptAutofillTracker();

  void OnJavaScriptChangedValue(const blink::WebFormControlElement& element,
                                const blink::WebString& old_value);

  // Invoked directly from Blink just prior to initiating DOM mousedown event
  // dispatch, before JavaScript receives the same signal.
  // `target_node` is the innermost hit-tested Blink node that receives the
  // mousedown event.
  void HandleMousedown(const blink::WebNode& target_node);

  // Clears all recorded changes and stops the detection timer.
  void Reset();

 private:
  friend class JavaScriptAutofillTrackerTestApi;

  // Analyzes the recorded changes in `js_logs_` to determine if they constitute
  // a JavaScript autofill event anchored on `trigger_element_id`. If so,
  // invokes `callback_`.
  void DetectJavaScriptAutofill(FieldRendererId trigger_element_id);

  // The owning frame.
  const raw_ref<blink::WebLocalFrame> web_frame_;

  // Callback to notify the owner of a detected JS autofill.
  DidDetectCallback callback_;

  // A rolling log of recent JavaScript-triggered value changes.
  std::vector<mojom::JavaScriptFieldModificationPtr> js_logs_;

  // Timer used to wait for a sequence of JS changes to complete before
  // analyzing them.
  base::OneShotTimer timer_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_JAVASCRIPT_AUTOFILL_TRACKER_H_
