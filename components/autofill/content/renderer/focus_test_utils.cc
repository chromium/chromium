// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <utility>

#include "base/strings/strcat.h"
#include "components/autofill/content/renderer/focus_test_utils.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_input_element.h"

namespace autofill::test {

FocusTestUtils::FocusTestUtils(
    ExecuteJavascriptFunction execute_java_script_function)
    : execute_java_script_function_(std::move(execute_java_script_function)) {}

FocusTestUtils::~FocusTestUtils() = default;

void FocusTestUtils::SetUpFocusLogging() {
  const char* js_str =
      "var event_log_element = document.createElement('input');"
      "event_log_element.type = 'text';"
      "event_log_element.id = 'event_log';"
      "document.body.appendChild(event_log_element);"
      "['blur', 'focus', 'change'].forEach(event_name => "
      "   document.addEventListener(event_name, function(event) {"
      "     event_log_element.value += event_name[0] + event.target.name;"
      "   }, true));";

  execute_java_script_function_.Run(js_str);
}

void FocusTestUtils::FocusElement(std::string_view element_id) {
  execute_java_script_function_.Run(
      base::StrCat({"document.getElementById('", element_id, "').focus();"}));
}

std::string FocusTestUtils::GetFocusLog(const blink::WebDocument& document) {
  blink::WebElement element =
      document.GetElementById(blink::WebString::FromUTF8("event_log"));
  if (!element) {
    return "event_log_element_id not found";
  }
  blink::WebInputElement input_element =
      element.DynamicTo<blink::WebInputElement>();
  if (!input_element) {
    return "event_log_element_id does not point to input element";
  }
  return input_element.Value().Utf8();
}

}  // namespace autofill::test
