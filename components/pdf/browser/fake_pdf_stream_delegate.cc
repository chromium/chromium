// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/fake_pdf_stream_delegate.h"

#include <utility>

#include "components/pdf/browser/pdf_stream_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace pdf {

// static
constexpr char FakePdfStreamDelegate::kDefaultStreamUrl[];

// static
constexpr char FakePdfStreamDelegate::kDefaultOriginalUrl[];

FakePdfStreamDelegate::FakePdfStreamDelegate() {
  StreamInfo info;
  info.stream_url = GURL(kDefaultStreamUrl);
  info.original_url = GURL(kDefaultOriginalUrl);
  stream_info_ = std::move(info);
}

FakePdfStreamDelegate::~FakePdfStreamDelegate() = default;

absl::optional<GURL> FakePdfStreamDelegate::MapToOriginalUrl(
    content::NavigationHandle& navigation_handle) {
  if (!stream_info_ || stream_info_->stream_url != navigation_handle.GetURL()) {
    return absl::nullopt;
  }

  return stream_info_->original_url;
}

absl::optional<PdfStreamDelegate::StreamInfo>
FakePdfStreamDelegate::GetStreamInfo(content::RenderFrameHost* embedder_frame) {
  EXPECT_TRUE(embedder_frame);
  return stream_info_;
}

}  // namespace pdf
