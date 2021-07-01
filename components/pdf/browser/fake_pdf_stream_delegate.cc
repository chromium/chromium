// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/fake_pdf_stream_delegate.h"

#include "components/pdf/browser/pdf_stream_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace pdf {

// static
constexpr char FakePdfStreamDelegate::kDefaultStreamUrl[];

// static
constexpr char FakePdfStreamDelegate::kDefaultOriginalUrl[];

FakePdfStreamDelegate::FakePdfStreamDelegate()
    : stream_info_({
          .stream_url = GURL(kDefaultStreamUrl),
          .original_url = GURL(kDefaultOriginalUrl),
      }) {}

FakePdfStreamDelegate::~FakePdfStreamDelegate() = default;

absl::optional<PdfStreamDelegate::StreamInfo>
FakePdfStreamDelegate::GetStreamInfo(content::WebContents* contents) {
  EXPECT_TRUE(contents);
  return stream_info_;
}

}  // namespace pdf
