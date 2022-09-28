// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PDF_CHROME_PDF_WEB_CONTENTS_HELPER_CLIENT_H_
#define CHROME_BROWSER_UI_PDF_CHROME_PDF_WEB_CONTENTS_HELPER_CLIENT_H_

#include "components/pdf/browser/pdf_web_contents_helper_client.h"

class ChromePDFWebContentsHelperClient
    : public pdf::PDFWebContentsHelperClient {
 public:
  ChromePDFWebContentsHelperClient();

  ChromePDFWebContentsHelperClient(const ChromePDFWebContentsHelperClient&) =
      delete;
  ChromePDFWebContentsHelperClient& operator=(
      const ChromePDFWebContentsHelperClient&) = delete;

  ~ChromePDFWebContentsHelperClient() override;

 private:
  // pdf::PDFWebContentsHelperClient:
  content::RenderFrameHost* FindPdfFrame(
      content::WebContents* contents) override;
  void UpdateContentRestrictions(content::RenderFrameHost* render_frame_host,
                                 int content_restrictions) override;
  void OnPDFHasUnsupportedFeature(content::WebContents* contents) override;
  void OnSaveURL(content::WebContents* contents) override;
  void SetPluginCanSave(content::RenderFrameHost* render_frame_host,
                        bool can_save) override;
};

#endif  // CHROME_BROWSER_UI_PDF_CHROME_PDF_WEB_CONTENTS_HELPER_CLIENT_H_
