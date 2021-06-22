// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_STREAM_DELEGATE_H_
#define COMPONENTS_PDF_BROWSER_PDF_STREAM_DELEGATE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
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
    GURL stream_url;
    GURL original_url;
  };

  PdfStreamDelegate();
  PdfStreamDelegate(const PdfStreamDelegate&) = delete;
  PdfStreamDelegate& operator=(const PdfStreamDelegate&) = delete;
  virtual ~PdfStreamDelegate();

  // Gets the stream information associated with the given `WebContents`.
  // Returns null if there is no associated stream.
  virtual absl::optional<StreamInfo> GetStreamInfo(
      content::WebContents* contents);
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PDF_STREAM_DELEGATE_H_
