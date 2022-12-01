// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_COMMON_WEB_MESSAGE_MOJOM_TRAITS_H_
#define COMPONENTS_JS_INJECTION_COMMON_WEB_MESSAGE_MOJOM_TRAITS_H_

#include <string>
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "base/test/scoped_path_override.h"
#include "components/js_injection/common/interfaces.mojom-shared.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace mojo {

template <>
struct UnionTraits<js_injection::mojom::JsWebMessageDataView,
                   blink::WebMessagePayload> {
  static std::u16string string_value(const blink::WebMessagePayload& payload) {
    return absl::get<std::u16string>(payload);
  }

  static js_injection::mojom::JsWebMessageDataView::Tag GetTag(
      const blink::WebMessagePayload& payload) {
    return absl::visit(
        base::Overloaded{
            [](const std::u16string&) {
              return js_injection::mojom::JsWebMessageDataView::Tag::
                  kStringValue;
            },
            [](const std::unique_ptr<blink::WebMessageArrayBufferPayload>&) {
              NOTREACHED() << "ArrayBufferPayload is not supported";
              return js_injection::mojom::JsWebMessageDataView::Tag::
                  kStringValue;
            }},
        payload);
  }

  static bool Read(js_injection::mojom::JsWebMessageDataView r,
                   blink::WebMessagePayload* out);
};

}  // namespace mojo

#endif
