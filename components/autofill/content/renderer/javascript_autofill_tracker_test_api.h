// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_JAVASCRIPT_AUTOFILL_TRACKER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_JAVASCRIPT_AUTOFILL_TRACKER_TEST_API_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/autofill/content/renderer/javascript_autofill_tracker.h"

namespace autofill {

// Exposes private members of JavaScriptAutofillTracker for testing.
class JavaScriptAutofillTrackerTestApi {
 public:
  explicit JavaScriptAutofillTrackerTestApi(JavaScriptAutofillTracker* tracker)
      : tracker_(*tracker) {}

  const std::vector<JavaScriptAutofillTracker::JsChangeRecord>& js_logs()
      const {
    return tracker_->js_logs_;
  }

 private:
  const raw_ref<JavaScriptAutofillTracker> tracker_;
};

inline JavaScriptAutofillTrackerTestApi test_api(
    JavaScriptAutofillTracker& tracker) {
  return JavaScriptAutofillTrackerTestApi(&tracker);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_JAVASCRIPT_AUTOFILL_TRACKER_TEST_API_H_
