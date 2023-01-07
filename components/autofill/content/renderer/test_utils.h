// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_TEST_UTILS_H_

#include "base/strings/string_piece.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/common/unique_ids.h"

namespace blink {
class WebFormControlElement;
class WebDocument;
class WebElement;
class WebFormElement;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace autofill {

using AllowNull = base::StrongAlias<struct AllowNullTag, bool>;

// Returns the element by its id attribute. May return an empty
// WebFormControlElement if |allow_null| is set.
blink::WebElement GetElementById(const blink::WebDocument& doc,
                                 base::StringPiece id,
                                 AllowNull allow_null = AllowNull(false));

// Returns the form control element by its id attribute. May return an empty
// WebFormControlElement if |allow_null| is set.
blink::WebFormControlElement GetFormControlElementById(
    const blink::WebDocument& doc,
    base::StringPiece id,
    AllowNull allow_null = AllowNull(false));

// Returns the form element by its id attribute. May return an empty
// WebFormElement if |allow_null| is set.
blink::WebFormElement GetFormElementById(
    const blink::WebDocument& doc,
    base::StringPiece id,
    AllowNull allow_null = AllowNull(false));

// Returns the WebLocalFrame that corresponds to the iframe element with the
// given |id|.
content::RenderFrame* GetIframeById(const blink::WebDocument& doc,
                                    base::StringPiece id,
                                    AllowNull allow_null = AllowNull(false));

// Returns the FrameToken of the iframe element with the given |id|.
FrameToken GetFrameToken(const blink::WebDocument& doc,
                         base::StringPiece id,
                         AllowNull allow_null = AllowNull(false));

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_TEST_UTILS_H_
