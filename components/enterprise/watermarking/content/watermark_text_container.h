// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_WATERMARKING_CONTENT_WATERMARK_TEXT_CONTAINER_H_
#define COMPONENTS_ENTERPRISE_WATERMARKING_CONTENT_WATERMARK_TEXT_CONTAINER_H_

#include "components/enterprise/watermarking/mojom/watermark.mojom-forward.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace enterprise_watermark {

class WatermarkTextContainer
    : public content::WebContentsUserData<WatermarkTextContainer> {
 public:
  explicit WatermarkTextContainer(content::WebContents* contents);
  ~WatermarkTextContainer() override;

  void SetWatermarkText(sk_sp<SkPicture> picture,
                        int block_width,
                        int block_height);

  // Returns nullptr if the `SkPicture` is not set, or if one of the block
  // dimensions is zero.
  watermark::mojom::WatermarkBlockPtr Serialize() const;

 private:
  friend class content::WebContentsUserData<WatermarkTextContainer>;

  sk_sp<SkPicture> picture_;
  int block_width_;
  int block_height_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace enterprise_watermark

#endif  // COMPONENTS_ENTERPRISE_WATERMARKING_CONTENT_WATERMARK_TEXT_CONTAINER_H_
