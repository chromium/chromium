// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_DOCUMENT_HELPER_CLIENT_H_
#define COMPONENTS_PDF_BROWSER_PDF_DOCUMENT_HELPER_CLIENT_H_

#include "services/screen_ai/buildflags/buildflags.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace gfx {
class SelectionBound;
}

namespace pdf {

class PDFDocumentHelperClient {
 public:
  virtual ~PDFDocumentHelperClient() = default;

  // Notifies that the document load completed successfully. This runs after
  // callbacks registered via
  // `PDFDocumentHelper::RegisterForDocumentLoadComplete()` run.
  virtual void OnDocumentLoadComplete(
      content::RenderFrameHost* render_frame_host) {}

  virtual void UpdateContentRestrictions(
      content::RenderFrameHost* render_frame_host,
      int content_restrictions) {}

  virtual void OnSaveURL() {}

  // Sets whether the PDF plugin can handle file saving internally.
  virtual void SetPluginCanSave(content::RenderFrameHost* render_frame_host,
                                bool can_save) {}

  // Lets the client observe scroll events. Only used for testing.
  virtual void OnDidScroll(const gfx::SelectionBound& start,
                           const gfx::SelectionBound& end) {}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // Notifies that PDF searchifier started processing pages.
  virtual void OnSearchifyStarted(content::RenderFrameHost* render_frame_host) {
  }
#endif
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PDF_DOCUMENT_HELPER_CLIENT_H_
