// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/test_utils.h"

#include "base/strings/strcat.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_remote_frame.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebString;

namespace autofill {

using AllowNull = base::StrongAlias<struct AllowNullTag, bool>;

WebElement GetElementById(const WebDocument& doc,
                          std::string_view id,
                          AllowNull allow_null) {
  WebElement e = doc.GetElementById(WebString::FromASCII(std::string(id)));
  CHECK(allow_null || e);
  return e;
}

blink::WebElement GetElementById(const blink::WebNode& node,
                                 std::string_view id,
                                 AllowNull allow_null) {
  WebElement e =
      node.QuerySelector(WebString::FromASCII(base::StrCat({"#", id})));
  CHECK(allow_null || e);
  return e;
}

content::RenderFrame* GetIframeById(const WebDocument& doc,
                                    std::string_view id,
                                    AllowNull allow_null) {
  WebElement iframe = GetElementById(doc, id, allow_null);
  CHECK(allow_null || iframe.HasHTMLTagName("iframe"));
  return iframe ? content::RenderFrame::FromWebFrame(
                      blink::WebFrame::FromFrameOwnerElement(iframe)
                          ->ToWebLocalFrame())
                : nullptr;
}

FrameToken GetFrameToken(const blink::WebDocument& doc,
                         std::string_view id,
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
