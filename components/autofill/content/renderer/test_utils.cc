// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/test_utils.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_remote_frame.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebString;

namespace autofill {

using AllowNull = base::StrongAlias<struct AllowNullTag, bool>;

WebElement GetElementById(const WebDocument& doc,
                          base::StringPiece id,
                          AllowNull allow_null) {
  WebElement e = doc.GetElementById(WebString::FromASCII(std::string(id)));
  CHECK(allow_null || !e.IsNull());
  return e;
}

WebFormControlElement GetFormControlElementById(const WebDocument& doc,
                                                base::StringPiece id,
                                                AllowNull allow_null) {
  return GetElementById(doc, id, allow_null).To<WebFormControlElement>();
}

WebFormElement GetFormElementById(const WebDocument& doc,
                                  base::StringPiece id,
                                  AllowNull allow_null) {
  return GetElementById(doc, id, allow_null).To<WebFormElement>();
}

content::RenderFrame* GetIframeById(const WebDocument& doc,
                                    base::StringPiece id,
                                    AllowNull allow_null) {
  WebElement iframe = GetElementById(doc, id, allow_null);
  CHECK(allow_null || iframe.HasHTMLTagName("iframe"));
  return !iframe.IsNull() ? content::RenderFrame::FromWebFrame(
                                blink::WebFrame::FromFrameOwnerElement(iframe)
                                    ->ToWebLocalFrame())
                          : nullptr;
}

FrameToken GetFrameToken(const blink::WebDocument& doc,
                         base::StringPiece id,
                         AllowNull allow_null) {
  WebElement iframe = GetElementById(doc, id, allow_null);
  CHECK(allow_null || iframe.HasHTMLTagName("iframe"));
  blink::WebFrame* frame = blink::WebFrame::FromFrameOwnerElement(iframe);
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
