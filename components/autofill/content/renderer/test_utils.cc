// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/test_utils.h"

#include "base/strings/strcat.h"
#include "base/types/strong_alias.h"
#include "content/public/renderer/render_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_remote_frame.h"

namespace autofill {

using ::blink::WebDocument;
using ::blink::WebElement;
using ::blink::WebFormControlElement;
using ::blink::WebFrame;
using ::blink::WebInputElement;
using ::blink::WebLocalFrame;
using ::blink::WebNode;
using ::blink::WebString;
using ::testing::AllOfArray;
using ::testing::Matcher;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::SafeMatcherCast;

namespace test {

Matcher<WebInputElement> WebInputElementEq(
    const WebFormControlElementDescription& description) {
  return SafeMatcherCast<WebInputElement>(WebFormControlElementEq(description));
}

Matcher<WebFormControlElement> WebFormControlElementEq(
    const WebFormControlElementDescription& description) {
  std::vector<Matcher<WebFormControlElement>> matchers;
  if (description.value.has_value()) {
    matchers.push_back(ResultOf(
        "value",
        [](const WebFormControlElement& element) {
          return element.Value().Utf8();
        },
        *description.value));
  }
  if (description.suggested_value.has_value()) {
    matchers.push_back(ResultOf(
        "suggested_value",
        [](const WebFormControlElement& element) {
          return element.SuggestedValue().Utf8();
        },
        *description.suggested_value));
  }
  if (description.is_autofilled) {
    matchers.push_back(Property("is_autofilled",
                                &WebFormControlElement::IsAutofilled,
                                *description.is_autofilled));
  }
  if (description.is_previewed) {
    matchers.push_back(Property("is_previewed",
                                &WebFormControlElement::IsPreviewed,
                                *description.is_previewed));
  }
  return AllOfArray(matchers);
}

}  // namespace test

using AllowNull = base::StrongAlias<struct AllowNullTag, bool>;

WebElement GetElementById(const WebDocument& doc,
                          std::string_view id,
                          AllowNull allow_null) {
  WebElement e = doc.GetElementById(WebString::FromAscii(std::string(id)));
  CHECK(allow_null || e);
  return e;
}

WebElement GetElementById(const WebNode& node,
                          std::string_view id,
                          AllowNull allow_null) {
  WebElement e =
      node.QuerySelector(WebString::FromAscii(base::StrCat({"#", id})));
  CHECK(allow_null || e);
  return e;
}

content::RenderFrame* GetIframeById(const WebDocument& doc,
                                    std::string_view id,
                                    AllowNull allow_null) {
  WebElement iframe = GetElementById(doc, id, allow_null);
  CHECK(allow_null || iframe.HasHTMLTagName("iframe"));
  return iframe
             ? content::RenderFrame::FromWebFrame(
                   WebFrame::FromFrameOwnerElement(iframe)->ToWebLocalFrame())
             : nullptr;
}

WebDocument GetIframeDocumentById(const WebDocument& doc,
                                  std::string_view id,
                                  AllowNull allow_null) {
  content::RenderFrame* render_frame = GetIframeById(doc, id, allow_null);
  CHECK(allow_null || render_frame);
  WebLocalFrame* web_local_frame =
      render_frame ? render_frame->GetWebFrame() : nullptr;
  CHECK(allow_null || web_local_frame);
  WebDocument child_document =
      web_local_frame ? web_local_frame->GetDocument() : WebDocument();
  CHECK(allow_null || child_document);
  return child_document;
}

FrameToken GetFrameToken(const WebDocument& doc,
                         std::string_view id,
                         AllowNull allow_null) {
  WebElement iframe = GetElementById(doc, id, allow_null);
  CHECK(allow_null || iframe.HasHTMLTagName("iframe"));
  WebFrame* frame = WebFrame::FromFrameOwnerElement(iframe);
  if (frame && frame->IsWebLocalFrame()) {
    return LocalFrameToken(
        frame->ToWebLocalFrame()->GetLocalFrameToken().value());
  } else if (frame && frame->IsWebRemoteFrame()) {
    return RemoteFrameToken(
        frame->ToWebRemoteFrame()->GetRemoteFrameToken().value());
  } else {
    CHECK(allow_null);
    return FrameToken();
  }
}

}  // namespace autofill
