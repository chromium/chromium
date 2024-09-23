// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PDF_CHROME_PDF_DOCUMENT_HELPER_CLIENT_H_
#define CHROME_BROWSER_UI_PDF_CHROME_PDF_DOCUMENT_HELPER_CLIENT_H_

#include "components/pdf/browser/pdf_document_helper_client.h"

class ChromePDFDocumentHelperClient : public pdf::PDFDocumentHelperClient {
 public:
  ChromePDFDocumentHelperClient();

  ChromePDFDocumentHelperClient(const ChromePDFDocumentHelperClient&) = delete;
  ChromePDFDocumentHelperClient& operator=(
      const ChromePDFDocumentHelperClient&) = delete;

  ~ChromePDFDocumentHelperClient() override;

 private:
  // pdf::PDFDocumentHelperClient:
  void UpdateContentRestrictions(content::RenderFrameHost* render_frame_host,
                                 int content_restrictions) override;
  void OnPDFHasUnsupportedFeature(content::WebContents* contents) override;
  void OnSaveURL(content::WebContents* contents) override;
  void SetPluginCanSave(content::RenderFrameHost* render_frame_host,
                        bool can_save) override;
};

#endif  // CHROME_BROWSER_UI_PDF_CHROME_PDF_DOCUMENT_HELPER_CLIENT_H_
