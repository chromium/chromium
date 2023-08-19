// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_FAKE_PDF_STREAM_DELEGATE_H_
#define COMPONENTS_PDF_BROWSER_FAKE_PDF_STREAM_DELEGATE_H_

#include "components/pdf/browser/pdf_stream_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace pdf {

class FakePdfStreamDelegate : public PdfStreamDelegate {
 public:
  static constexpr char kDefaultStreamUrl[] =
      "chrome-extension://id/stream-url";
  static constexpr char kDefaultOriginalUrl[] = "https://example.test/fake.pdf";

  FakePdfStreamDelegate();
  FakePdfStreamDelegate(const FakePdfStreamDelegate&) = delete;
  FakePdfStreamDelegate& operator=(const FakePdfStreamDelegate&) = delete;
  ~FakePdfStreamDelegate() override;

  // `PdfStreamDelegate`:
  absl::optional<GURL> MapToOriginalUrl(
      content::NavigationHandle& navigation_handle) override;
  absl::optional<StreamInfo> GetStreamInfo(
      content::RenderFrameHost* embedder_frame) override;

  void clear_stream_info() { stream_info_.reset(); }

 private:
  absl::optional<StreamInfo> stream_info_;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_FAKE_PDF_STREAM_DELEGATE_H_
