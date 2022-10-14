// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_COMMON_WEB_MESSAGE_MOJOM_TRAITS_H_
#define COMPONENTS_JS_INJECTION_COMMON_WEB_MESSAGE_MOJOM_TRAITS_H_

#include <string>
#include "components/js_injection/common/interfaces.mojom-shared.h"
#include "components/js_injection/common/web_message.h"
#include "mojo/public/cpp/bindings/union_traits.h"

namespace mojo {

template <>
struct UnionTraits<js_injection::mojom::JsWebMessageDataView,
                   js_injection::JsWebMessage> {
  static std::u16string string_value(
      const js_injection::JsWebMessage& message) {
    return message.string;
  }

  static js_injection::mojom::JsWebMessageDataView::Tag GetTag(
      const js_injection::JsWebMessage& input) {
    return js_injection::mojom::JsWebMessageDataView::Tag::kStringValue;
  }

  static bool Read(js_injection::mojom::JsWebMessageDataView r,
                   js_injection::JsWebMessage* out);
};

}  // namespace mojo

#endif
