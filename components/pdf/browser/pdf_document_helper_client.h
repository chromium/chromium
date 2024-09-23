// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_DOCUMENT_HELPER_CLIENT_H_
#define COMPONENTS_PDF_BROWSER_PDF_DOCUMENT_HELPER_CLIENT_H_

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace gfx {
class SelectionBound;
}

namespace pdf {

class PDFDocumentHelperClient {
 public:
  virtual ~PDFDocumentHelperClient() = default;

  virtual void UpdateContentRestrictions(
      content::RenderFrameHost* render_frame_host,
      int content_restrictions) = 0;

  virtual void OnPDFHasUnsupportedFeature(content::WebContents* contents) = 0;

  virtual void OnSaveURL(content::WebContents* contents) = 0;

  // Sets whether the PDF plugin can handle file saving internally.
  virtual void SetPluginCanSave(content::RenderFrameHost* render_frame_host,
                                bool can_save) = 0;

  // Lets the client observe scroll events. Only used for testing.
  virtual void OnDidScroll(const gfx::SelectionBound& start,
                           const gfx::SelectionBound& end) {}
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PDF_DOCUMENT_HELPER_CLIENT_H_
