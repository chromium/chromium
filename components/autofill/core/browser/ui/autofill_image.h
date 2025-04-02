// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_H_

#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace autofill {

// Represents an image fetched from the network for Autofill.
struct AutofillImage {
 public:
  AutofillImage(const GURL& image_url, const gfx::Image& image);
  AutofillImage(const AutofillImage& other);
  ~AutofillImage();

  // The url to fetch the image.
  GURL image_url;

  // The image fetched from the network.
  gfx::Image image;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_IMAGE_H_
