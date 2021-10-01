// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/test_utils.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"

using blink::WebDocument;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebString;

namespace autofill {

using AllowNull = base::StrongAlias<struct AllowNullTag, bool>;

WebFormControlElement GetFormControlElementById(const WebDocument& doc,
                                                base::StringPiece id,
                                                AllowNull allow_null) {
  auto queried_form_control =
      doc.GetElementById(WebString::FromASCII(std::string(id)))
          .To<WebFormControlElement>();
  CHECK(allow_null || !queried_form_control.IsNull());
  return queried_form_control;
}

WebFormElement GetFormElementById(const WebDocument& doc,
                                  base::StringPiece id,
                                  AllowNull allow_null) {
  auto queried_form =
      doc.GetElementById(blink::WebString::FromASCII(std::string(id)))
          .To<WebFormElement>();
  CHECK(allow_null || !queried_form.IsNull());
  return queried_form;
}

}  // namespace autofill
