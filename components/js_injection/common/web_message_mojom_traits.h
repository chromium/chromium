// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_COMMON_WEB_MESSAGE_MOJOM_TRAITS_H_
#define COMPONENTS_JS_INJECTION_COMMON_WEB_MESSAGE_MOJOM_TRAITS_H_

#include <string>

#include "components/js_injection/common/interfaces.mojom-shared.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace mojo {

template <>
struct UnionTraits<js_injection::mojom::JsWebMessageDataView,
                   blink::WebMessagePayload> {
  static std::u16string string_value(blink::WebMessagePayload& payload) {
    return absl::get<std::u16string>(payload);
  }

  static std::unique_ptr<blink::WebMessageArrayBufferPayload>
  array_buffer_value(blink::WebMessagePayload& payload) {
    return std::move(
        absl::get<std::unique_ptr<blink::WebMessageArrayBufferPayload>>(
            payload));
  }

  static js_injection::mojom::JsWebMessageDataView::Tag GetTag(
      const blink::WebMessagePayload& payload);

  static bool Read(js_injection::mojom::JsWebMessageDataView r,
                   blink::WebMessagePayload* out);
};

}  // namespace mojo

#endif
