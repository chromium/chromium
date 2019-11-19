// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/autofill/password_generation_test_utils.h"

#include <base/strings/utf_string_conversions.h>
#include "base/strings/stringprintf.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/signatures_util.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_element.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebString;
using blink::WebVector;

namespace autofill {

namespace {

// Events that should be triggered when Chrome fills a field.
const char* const kEvents[] = {"focus",  "keydown", "input",
                               "change", "keyup",   "blur"};

// Returns renderer id of WebInput element with id attribute |input_id|.
uint32_t GetRendererId(WebDocument document, const char* input_id) {
  WebElement element = document.GetElementById(WebString::FromUTF8(input_id));
  auto* input = ToWebInputElement(&element);
  return input->UniqueRendererFormControlId();
}

}  // namespace

void SetFoundFormEligibleForGeneration(
    PasswordGenerationAgent* generation_agent,
    WebDocument document,
    const char* new_password_id,
    const char* cofirm_password_id) {
  PasswordFormGenerationData data;
  data.new_password_renderer_id = GetRendererId(document, new_password_id);
  if (cofirm_password_id) {
    data.confirmation_password_renderer_id =
        GetRendererId(document, cofirm_password_id);
  }

  generation_agent->FoundFormEligibleForGeneration(data);
}

// Creates script that registers event listeners for |element_name| field. To
// check whether the listeners are called, check that the variables from
// |variables_to_check| are set to 1.
std::string CreateScriptToRegisterListeners(
    const char* const element_name,
    std::vector<base::string16>* variables_to_check) {
  DCHECK(variables_to_check);
  std::string element = element_name;

  std::string all_scripts = "<script>";
  for (const char* const event : kEvents) {
    std::string script = base::StringPrintf(
        "%s_%s_event = 0;"
        "document.getElementById('%s').on%s = function() {"
        "  %s_%s_event = 1;"
        "};",
        element_name, event, element_name, event, element_name, event);
    all_scripts += script;
    variables_to_check->push_back(base::UTF8ToUTF16(
        base::StringPrintf("%s_%s_event", element_name, event)));
  }

  all_scripts += "</script>";
  return all_scripts;
}

}  // namespace autofill
