// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_stream_delegate.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace pdf {

PdfStreamDelegate::PdfStreamDelegate() = default;
PdfStreamDelegate::~PdfStreamDelegate() = default;

absl::optional<GURL> PdfStreamDelegate::MapToOriginalUrl(
    content::NavigationHandle& navigation_handle) {
  return absl::nullopt;
}

absl::optional<PdfStreamDelegate::StreamInfo> PdfStreamDelegate::GetStreamInfo(
    content::RenderFrameHost* embedder_frame) {
  return absl::nullopt;
}

PdfStreamDelegate::StreamInfo::StreamInfo() = default;
PdfStreamDelegate::StreamInfo::StreamInfo(const StreamInfo&) = default;
PdfStreamDelegate::StreamInfo::StreamInfo(StreamInfo&&) = default;
PdfStreamDelegate::StreamInfo& PdfStreamDelegate::StreamInfo::operator=(
    const StreamInfo&) = default;
PdfStreamDelegate::StreamInfo& PdfStreamDelegate::StreamInfo::operator=(
    StreamInfo&&) = default;
PdfStreamDelegate::StreamInfo::~StreamInfo() = default;

}  // namespace pdf
