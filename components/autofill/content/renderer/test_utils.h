// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_TEST_UTILS_H_

#include <concepts>
#include <string_view>

#include "base/types/strong_alias.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"

namespace blink {
class WebDocument;
class WebNode;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace autofill {

namespace internal {
template <typename T>
concept SupportsLookupById = (std::derived_from<T, blink::WebDocument> ||
                              std::derived_from<T, blink::WebNode>);
}  // namespace internal

using AllowNull = base::StrongAlias<struct AllowNullTag, bool>;

// Returns the element by its id attribute. May return an empty
// `WebFormControlElement` if `allow_null` is set. Note that the function taking
// a `WebNode` can be used to find elements inside a shadow root.
blink::WebElement GetElementById(const blink::WebDocument& doc,
                                 std::string_view id,
                                 AllowNull allow_null = AllowNull(false));
blink::WebElement GetElementById(const blink::WebNode& node,
                                 std::string_view id,
                                 AllowNull allow_null = AllowNull(false));

// Returns the form control element by its id attribute. May return an empty
// WebFormControlElement if `allow_null` is set.
template <typename T>
  requires(internal::SupportsLookupById<T>)
blink::WebFormControlElement GetFormControlElementById(
    const T& t,
    std::string_view id,
    AllowNull allow_null = AllowNull(false)) {
  blink::WebFormControlElement e =
      GetElementById(t, id, allow_null)
          .template DynamicTo<blink::WebFormControlElement>();
  CHECK(allow_null || e);
  return e;
}

// Returns the form element by its id attribute. May return an empty
// WebFormElement if `allow_null` is set.
template <typename T>
  requires(internal::SupportsLookupById<T>)
blink::WebFormElement GetFormElementById(
    const T& t,
    std::string_view id,
    AllowNull allow_null = AllowNull(false)) {
  blink::WebFormElement e = GetElementById(t, id, allow_null)
                                .template DynamicTo<blink::WebFormElement>();
  CHECK(allow_null || e);
  return e;
}

// Returns the WebLocalFrame that corresponds to the iframe element with the
// given |id|.
content::RenderFrame* GetIframeById(const blink::WebDocument& doc,
                                    std::string_view id,
                                    AllowNull allow_null = AllowNull(false));

// Returns the FrameToken of the iframe element with the given |id|.
FrameToken GetFrameToken(const blink::WebDocument& doc,
                         std::string_view id,
                         AllowNull allow_null = AllowNull(false));

// Returns how often AskForValuesToFill is expected to be called for a focus
// change completion that is triggered by a click or tap.
int AskForValuesToFillCallsOnFocusChangeByClickOrTap();

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_TEST_UTILS_H_
