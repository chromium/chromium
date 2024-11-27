// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_WATERMARKING_CONTENT_WATERMARK_TEXT_CONTAINER_H_
#define COMPONENTS_ENTERPRISE_WATERMARKING_CONTENT_WATERMARK_TEXT_CONTAINER_H_

#include "content/public/browser/web_contents_user_data.h"

namespace enterprise_watermark {

class WatermarkTextContainer
    : public content::WebContentsUserData<WatermarkTextContainer> {
 public:
  explicit WatermarkTextContainer(content::WebContents* contents);
  ~WatermarkTextContainer() override = default;

  void SetWatermarkText(std::string watermark_text);
  const std::string& watermark_text() const;

 private:
  friend class content::WebContentsUserData<WatermarkTextContainer>;

  std::string watermark_text_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace enterprise_watermark

#endif  // COMPONENTS_ENTERPRISE_WATERMARKING_CONTENT_WATERMARK_TEXT_CONTAINER_H_
