// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_WEB_CONTENTS_HELPER_CLIENT_H_
#define COMPONENTS_PDF_BROWSER_PDF_WEB_CONTENTS_HELPER_CLIENT_H_

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace pdf {

class PDFWebContentsHelperClient {
 public:
  virtual ~PDFWebContentsHelperClient() = default;

  virtual content::RenderFrameHost* FindPdfFrame(
      content::WebContents* contents) = 0;

  virtual void UpdateContentRestrictions(
      content::RenderFrameHost* render_frame_host,
      int content_restrictions) = 0;

  virtual void OnPDFHasUnsupportedFeature(content::WebContents* contents) = 0;

  virtual void OnSaveURL(content::WebContents* contents) = 0;

  // Sets whether the PDF plugin can handle file saving internally.
  virtual void SetPluginCanSave(content::RenderFrameHost* render_frame_host,
                                bool can_save) = 0;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PDF_WEB_CONTENTS_HELPER_CLIENT_H_
