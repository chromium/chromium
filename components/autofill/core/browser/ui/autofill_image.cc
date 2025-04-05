// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_image.h"

namespace autofill {

AutofillImage::AutofillImage(const GURL& image_url, const gfx::Image& image)
    : image_url(image_url), image(image) {}

AutofillImage::AutofillImage(const AutofillImage& other) = default;

AutofillImage::~AutofillImage() = default;

}  // namespace autofill
