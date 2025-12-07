// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_WATERMARKING_WATERMARK_TEST_UTILS_H_
#define COMPONENTS_ENTERPRISE_WATERMARKING_WATERMARK_TEST_UTILS_H_

#include <string>

#include "components/enterprise/watermarking/mojom/watermark.mojom-forward.h"
#include "third_party/skia/include/core/SkSize.h"

namespace enterprise_watermark {

watermark::mojom::WatermarkBlockPtr MakeTestWatermarkBlock(
    const std::string& watermark_text,
    const SkSize watermark_size);

}  // namespace enterprise_watermark

#endif  // COMPONENTS_ENTERPRISE_WATERMARKING_WATERMARK_TEST_UTILS_H_
