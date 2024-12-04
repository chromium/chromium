// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/watermarking/content/watermark_text_container.h"

namespace enterprise_watermark {

WatermarkTextContainer::WatermarkTextContainer(content::WebContents* contents)
    : content::WebContentsUserData<WatermarkTextContainer>(*contents),
      block_width_(0),
      block_height_(0) {}

void WatermarkTextContainer::SetWatermarkText(sk_sp<SkPicture> picture,
                                              int block_width,
                                              int block_height) {
  picture_ = picture;
  block_width_ = block_width;
  block_height_ = block_height;
}

WatermarkTextContainer::~WatermarkTextContainer() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(WatermarkTextContainer);

}  // namespace enterprise_watermark
