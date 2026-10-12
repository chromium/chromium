// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/js_injection/common/web_message_mojom_traits.h"

#include <string>
#include <variant>

#include "components/js_injection/common/interfaces.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace mojo {

// static
bool StructTraits<js_injection::mojom::JsWebMessageArrayBufferValueDataView,
                  std::unique_ptr<blink::WebMessageArrayBufferPayload>>::
    Read(js_injection::mojom::JsWebMessageArrayBufferValueDataView r,
         std::unique_ptr<blink::WebMessageArrayBufferPayload>* out) {
  mojo_base::BigBufferView big_buffer_view;
  if (!r.ReadArrayBufferValue(&big_buffer_view)) {
    return false;
  }
  std::optional<size_t> max_byte_length = r.javascript_resize_limit();
  *out = blink::WebMessageArrayBufferPayload::CreateFromBigBuffer(
      mojo_base::BigBufferView::ToBigBuffer(std::move(big_buffer_view)),
      max_byte_length);
  return true;
}

// static
js_injection::mojom::JsWebMessageDataView::Tag UnionTraits<
    js_injection::mojom::JsWebMessageDataView,
    blink::WebMessagePayload>::GetTag(const blink::WebMessagePayload& payload) {
  return std::visit(
      absl::Overload{
          [](const std::u16string&) {
            return js_injection::mojom::JsWebMessageDataView::Tag::kStringValue;
          },
          [](const std::unique_ptr<blink::WebMessageArrayBufferPayload>&) {
            return js_injection::mojom::JsWebMessageDataView::Tag::
                kArrayBufferValue;
          },
#if BUILDFLAG(IS_ANDROID)
          [](const base::UnsafeSharedMemoryRegion&) {
            return js_injection::mojom::JsWebMessageDataView::Tag::
                kSharedArrayBufferValue;
          },
#endif
      },
      payload);
}

// static
bool UnionTraits<
    js_injection::mojom::JsWebMessageDataView,
    blink::WebMessagePayload>::Read(js_injection::mojom::JsWebMessageDataView r,
                                    blink::WebMessagePayload* out) {
  switch (r.tag()) {
    case js_injection::mojom::JsWebMessageDataView::Tag::kStringValue: {
      std::u16string string_value;
      if (!r.ReadStringValue(&string_value)) {
        return false;
      }
      out->emplace<std::u16string>(std::move(string_value));
      return true;
    }
    case js_injection::mojom::JsWebMessageDataView::Tag::kArrayBufferValue: {
      std::unique_ptr<blink::WebMessageArrayBufferPayload> array_buffer_value;
      if (!r.ReadArrayBufferValue(&array_buffer_value)) {
        return false;
      }
      out->emplace<std::unique_ptr<blink::WebMessageArrayBufferPayload>>(
          std::move(array_buffer_value));
      return true;
    }
#if BUILDFLAG(IS_ANDROID)
    case js_injection::mojom::JsWebMessageDataView::Tag::
        kSharedArrayBufferValue: {
      base::UnsafeSharedMemoryRegion shared_memory;
      if (!r.ReadSharedArrayBufferValue(&shared_memory)) {
        return false;
      }
      out->emplace<base::UnsafeSharedMemoryRegion>(std::move(shared_memory));
      return true;
    }
#endif
  }
  return false;
}

}  // namespace mojo
