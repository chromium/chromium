// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/js_injection/common/web_message_mojom_traits.h"

#include <string>
#include "components/js_injection/common/interfaces.mojom.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace mojo {

// static
bool UnionTraits<
    js_injection::mojom::JsWebMessageDataView,
    blink::WebMessagePayload>::Read(js_injection::mojom::JsWebMessageDataView r,
                                    blink::WebMessagePayload* out) {
  std::u16string string_value;
  if (!r.ReadStringValue(&string_value))
    return false;

  out->emplace<std::u16string>(std::move(string_value));

  return true;
}

}  // namespace mojo
