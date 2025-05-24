// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/watermarking/content/watermark_text_container.h"

#include <utility>

#include "components/enterprise/watermarking/mojom/watermark.mojom.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkStream.h"

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

watermark::mojom::WatermarkBlockPtr WatermarkTextContainer::Serialize() const {
  if (!picture_ || block_width_ == 0 || block_height_ == 0) {
    return nullptr;
  }
  SkDynamicMemoryWStream stream;
  SkSerialProcs procs;
  picture_->serialize(&stream, &procs);

  base::MappedReadOnlyRegion region_mapping =
      base::ReadOnlySharedMemoryRegion::Create(stream.bytesWritten());
  if (!region_mapping.IsValid()) {
    return nullptr;
  }
  stream.copyTo(region_mapping.mapping.memory());
  return watermark::mojom::WatermarkBlockPtr(std::in_place,
                                             std::move(region_mapping.region),
                                             block_width_, block_height_);
}

WatermarkTextContainer::~WatermarkTextContainer() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(WatermarkTextContainer);

}  // namespace enterprise_watermark
