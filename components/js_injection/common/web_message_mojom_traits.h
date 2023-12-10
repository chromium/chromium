// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_COMMON_WEB_MESSAGE_MOJOM_TRAITS_H_
#define COMPONENTS_JS_INJECTION_COMMON_WEB_MESSAGE_MOJOM_TRAITS_H_

#include <string>

#include "components/js_injection/common/interfaces.mojom-shared.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace mojo {

template <>
struct StructTraits<js_injection::mojom::JsWebMessageArrayBufferValueDataView,
                    std::unique_ptr<blink::WebMessageArrayBufferPayload>> {
  static mojo_base::BigBuffer array_buffer_value(
      const std::unique_ptr<blink::WebMessageArrayBufferPayload>&
          array_buffer) {
    auto big_buffer = mojo_base::BigBuffer(array_buffer->GetLength());
    array_buffer->CopyInto(big_buffer);
    return big_buffer;
  }

  static bool is_resizable_by_user_javascript(
      const std::unique_ptr<blink::WebMessageArrayBufferPayload>&
          array_buffer) {
    return array_buffer->GetIsResizableByUserJavaScript();
  }

  static size_t max_byte_length(
      const std::unique_ptr<blink::WebMessageArrayBufferPayload>&
          array_buffer) {
    return array_buffer->GetMaxByteLength();
  }

  static bool Read(js_injection::mojom::JsWebMessageArrayBufferValueDataView r,
                   std::unique_ptr<blink::WebMessageArrayBufferPayload>* out);
};

template <>
struct UnionTraits<js_injection::mojom::JsWebMessageDataView,
                   blink::WebMessagePayload> {
  static const std::u16string& string_value(
      const blink::WebMessagePayload& payload) {
    return absl::get<std::u16string>(payload);
  }

  static const std::unique_ptr<blink::WebMessageArrayBufferPayload>&
  array_buffer_value(const blink::WebMessagePayload& payload) {
    return absl::get<std::unique_ptr<blink::WebMessageArrayBufferPayload>>(
        payload);
  }

  static js_injection::mojom::JsWebMessageDataView::Tag GetTag(
      const blink::WebMessagePayload& payload);

  static bool Read(js_injection::mojom::JsWebMessageDataView r,
                   blink::WebMessagePayload* out);
};

}  // namespace mojo

#endif  // COMPONENTS_JS_INJECTION_COMMON_WEB_MESSAGE_MOJOM_TRAITS_H_
