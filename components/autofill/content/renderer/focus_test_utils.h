// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FOCUS_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FOCUS_TEST_UTILS_H_

#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "third_party/blink/public/web/web_document.h"

namespace autofill {

namespace test {

class FocusTestUtils {
  // Creates a result input field in the HTML body, then sets-up event listeners
  // for the focus, change and blur events to track the order of events emitted
  // for the fields. Ex. -> 'f0c0b0f1c1b1' would mean the following:
  // 1. Focus event for the field with name '0'
  // 2. Change event for the field with name '0'
  // 3. Blur event for the field with name '0'
  // 4. Focus event for the field with name '1'
  // 5. Change event for the field with name '1'
  // 6. Blur event for field with name '1'
 public:
  using ExecuteJavascriptFunction =
      base::RepeatingCallback<void(std::string_view)>;

  explicit FocusTestUtils(
      ExecuteJavascriptFunction execute_java_script_function);
  ~FocusTestUtils();
  FocusTestUtils(const FocusTestUtils&) = delete;
  FocusTestUtils& operator=(const FocusTestUtils&) = delete;

  // Creates a result input element and sets up event listeners for focus,
  // blur and change events for the form elements.
  void SetUpFocusLogging();

  // Emits focus event for the given field with id `element_id`.
  void FocusElement(std::string_view element_id);

  // Returns the sequence of focus events (see class description).
  std::string GetFocusLog(const blink::WebDocument& document);

 private:
  ExecuteJavascriptFunction execute_java_script_function_;
};

}  // namespace test
}  // namespace autofill

#endif  //  COMPONENTS_AUTOFILL_CONTENT_RENDERER_FOCUS_TEST_UTILS_H_
