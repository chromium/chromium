// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/watermarking/content/watermark_text_container.h"

namespace enterprise_watermark {

WatermarkTextContainer::WatermarkTextContainer(content::WebContents* contents)
    : content::WebContentsUserData<WatermarkTextContainer>(*contents) {}

void WatermarkTextContainer::SetWatermarkText(std::string watermark_text) {
  watermark_text_ = std::move(watermark_text);
}

const std::string& WatermarkTextContainer::watermark_text() const {
  return watermark_text_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WatermarkTextContainer);

}  // namespace enterprise_watermark
