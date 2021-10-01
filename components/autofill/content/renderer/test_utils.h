// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_TEST_UTILS_H_

#include "base/strings/string_piece.h"
#include "base/types/strong_alias.h"

namespace blink {
class WebFormControlElement;
class WebDocument;
class WebFormElement;
}  // namespace blink

namespace autofill {

using AllowNull = base::StrongAlias<struct AllowNullTag, bool>;

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
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_TEST_UTILS_H_
