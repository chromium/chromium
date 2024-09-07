// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_FAKE_PDF_STREAM_DELEGATE_H_
#define COMPONENTS_PDF_BROWSER_FAKE_PDF_STREAM_DELEGATE_H_

#include <optional>

#include "components/pdf/browser/pdf_stream_delegate.h"

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
  std::optional<GURL> MapToOriginalUrl(
      content::NavigationHandle& navigation_handle) override;
  std::optional<StreamInfo> GetStreamInfo(
      content::RenderFrameHost* embedder_frame) override;
  void OnPdfEmbedderSandboxed(
      content::FrameTreeNodeId frame_tree_node_id) override;
  bool ShouldAllowPdfFrameNavigation(
      content::NavigationHandle* navigation_handle) override;

  void clear_stream_info() { stream_info_.reset(); }

  void set_should_allow_pdf_frame_navigation(bool should_allow) {
    should_allow_pdf_frame_navigation_ = should_allow;
  }

 private:
  bool should_allow_pdf_frame_navigation_ = true;
  std::optional<StreamInfo> stream_info_;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_FAKE_PDF_STREAM_DELEGATE_H_
