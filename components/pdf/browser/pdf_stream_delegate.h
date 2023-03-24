// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_STREAM_DELEGATE_H_
#define COMPONENTS_PDF_BROWSER_PDF_STREAM_DELEGATE_H_

#include <string>

#include "base/memory/raw_ptr_exclusion.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace content {
class WebContents;
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

    // Script to be injected into the internal plugin frame. This should point
    // at an immutable string with static storage duration.
    // This field is not a raw_ptr<> because it was filtered by the rewriter
    // for: #union
    RAW_PTR_EXCLUSION const std::string* injected_script = nullptr;

    SkColor background_color = SK_ColorTRANSPARENT;
    bool full_frame = false;
    bool allow_javascript = false;
  };

  PdfStreamDelegate();
  PdfStreamDelegate(const PdfStreamDelegate&) = delete;
  PdfStreamDelegate& operator=(const PdfStreamDelegate&) = delete;
  virtual ~PdfStreamDelegate();

  // Maps the incoming stream URL to the original URL. This method should
  // associate a `StreamInfo` with the given `WebContents`, for later retrieval
  // by `GetStreamInfo()`.
  virtual absl::optional<GURL> MapToOriginalUrl(content::WebContents* contents,
                                                const GURL& stream_url);

  // Gets the stream information associated with the given `WebContents`.
  // Returns null if there is no associated stream.
  virtual absl::optional<StreamInfo> GetStreamInfo(
      content::WebContents* contents);
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PDF_STREAM_DELEGATE_H_
