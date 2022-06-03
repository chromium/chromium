// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/strings/stringprintf.h"
#include "components/autofill/content/renderer/focus_test_utils.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_input_element.h"

namespace autofill {
namespace test {

FocusTestUtils::FocusTestUtils(
    base::RepeatingCallback<void(const char*)> execute_java_script_function)
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

void FocusTestUtils::FocusElement(const char* element_id) {
  std::string js_str =
      base::StringPrintf("document.getElementById('%s').focus();", element_id);
  execute_java_script_function_.Run(js_str.c_str());
}

std::string FocusTestUtils::GetFocusLog(const blink::WebDocument& document) {
  blink::WebElement element =
      document.GetElementById(blink::WebString::FromUTF8("event_log"));
  if (element.IsNull())
    return "event_log_element_id not found";
  blink::WebInputElement* input_element = blink::ToWebInputElement(&element);
  if (!input_element)
    return "event_log_element_id does not point to input element";
  return input_element->Value().Utf8();
}

}  // namespace test

}  // namespace autofill
