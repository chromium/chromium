// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/js_injection/common/web_message_mojom_traits.h"

#include <string>

#include "base/functional/overloaded.h"
#include "components/js_injection/common/interfaces.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace mojo {

// static
js_injection::mojom::JsWebMessageDataView::Tag UnionTraits<
    js_injection::mojom::JsWebMessageDataView,
    blink::WebMessagePayload>::GetTag(const blink::WebMessagePayload& payload) {
  return absl::visit(
      base::Overloaded{
          [](const std::u16string&) {
            return js_injection::mojom::JsWebMessageDataView::Tag::kStringValue;
          },
          [](const std::unique_ptr<blink::WebMessageArrayBufferPayload>&) {
            return js_injection::mojom::JsWebMessageDataView::Tag::
                kArrayBufferValue;
          }},
      payload);
}

// static
bool UnionTraits<
    js_injection::mojom::JsWebMessageDataView,
    blink::WebMessagePayload>::Read(js_injection::mojom::JsWebMessageDataView r,
                                    blink::WebMessagePayload* out) {
  if (r.is_string_value()) {
    std::u16string string_value;
    if (!r.ReadStringValue(&string_value))
      return false;
    out->emplace<std::u16string>(std::move(string_value));
  } else if (r.is_array_buffer_value()) {
    mojo_base::BigBufferView big_buffer_view;
    if (!r.ReadArrayBufferValue(&big_buffer_view))
      return false;
    out->emplace<std::unique_ptr<blink::WebMessageArrayBufferPayload>>(
        blink::WebMessageArrayBufferPayload::CreateFromBigBuffer(
            mojo_base::BigBufferView::ToBigBuffer(std::move(big_buffer_view))));
  } else {
    return false;
  }

  return true;
}

}  // namespace mojo
