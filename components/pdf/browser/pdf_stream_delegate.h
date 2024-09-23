// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_STREAM_DELEGATE_H_
#define COMPONENTS_PDF_BROWSER_PDF_STREAM_DELEGATE_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr_exclusion.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace pdf {

// Delegate for obtaining information from the `extensions::StreamContainer` for
// the PDF viewer. This avoids a dependency on `//extensions/browser`, which
// would be a layering violation.
class PdfStreamDelegate {
 public:
  struct StreamInfo {
    StreamInfo();
    StreamInfo(const StreamInfo&);
    StreamInfo(StreamInfo&&);
    StreamInfo& operator=(const StreamInfo&);
    StreamInfo& operator=(StreamInfo&&);
    ~StreamInfo();

    GURL stream_url;
    GURL original_url;

    // Script to be injected into the internal plugin frame.
    // RAW_PTR_EXCLUSION: Points to an immutable string with static storage
    // duration.
    RAW_PTR_EXCLUSION const std::string* injected_script = nullptr;

    SkColor background_color = SK_ColorTRANSPARENT;
    bool full_frame = false;
    bool allow_javascript = false;
    bool use_skia = false;
    bool require_corp = false;
  };

  virtual ~PdfStreamDelegate() = default;

  // Maps the navigation to the original URL. This method should associate a
  // `StreamInfo` with the `blink::Document` for `navigation_handle`'s parent
  // `RenderFrameHost`, for later retrieval by `GetStreamInfo()`.
  virtual std::optional<GURL> MapToOriginalUrl(
      content::NavigationHandle& navigation_handle) = 0;

  // Gets the stream information associated with the given `RenderFrameHost`.
  // The frame must be a PDF extension frame or Print Preview's frame.
  // Returns null if there is no associated stream or if `embedder_frame` is
  // `nullptr`.
  virtual std::optional<StreamInfo> GetStreamInfo(
      content::RenderFrameHost* embedder_frame) = 0;

  // Called after calculating sandbox flags for the PDF embedder frame and it's
  // determined that the frame is sandboxed. This signals that the PDF
  // navigation will fail and gives `PdfStreamDelegate` a chance to clean up.
  virtual void OnPdfEmbedderSandboxed(
      content::FrameTreeNodeId frame_tree_node_id) = 0;

  // Determines whether navigation attempts in the PDF frames should be allowed.
  // Navigation attempts in PDF extension and content frames should be canceled
  // if they are not related to PDF viewer setup.
  virtual bool ShouldAllowPdfFrameNavigation(
      content::NavigationHandle* navigation_handle) = 0;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PDF_STREAM_DELEGATE_H_
