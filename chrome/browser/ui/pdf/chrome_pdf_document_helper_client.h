// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PDF_CHROME_PDF_DOCUMENT_HELPER_CLIENT_H_
#define CHROME_BROWSER_UI_PDF_CHROME_PDF_DOCUMENT_HELPER_CLIENT_H_

#include <string>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "components/pdf/browser/pdf_document_helper_client.h"
#include "content/public/browser/global_routing_id.h"

class ChromePDFDocumentHelperClient : public pdf::PDFDocumentHelperClient {
 public:
  ChromePDFDocumentHelperClient();

  ChromePDFDocumentHelperClient(const ChromePDFDocumentHelperClient&) = delete;
  ChromePDFDocumentHelperClient& operator=(
      const ChromePDFDocumentHelperClient&) = delete;

  ~ChromePDFDocumentHelperClient() override;

 private:
  // pdf::PDFDocumentHelperClient:
  void OnDocumentLoadComplete(
      content::RenderFrameHost* render_frame_host) override;
  void UpdateContentRestrictions(content::RenderFrameHost* render_frame_host,
                                 int content_restrictions) override;
  void OnSaveURL() override;
  void SetPluginCanSave(content::RenderFrameHost* render_frame_host,
                        bool can_save) override;
  void OnSearchifyStarted(content::RenderFrameHost* render_frame_host) override;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  void OnPdfTextExtracted(content::GlobalRenderFrameHostId render_frame_host_id,
                          const std::u16string& text);

  base::WeakPtrFactory<ChromePDFDocumentHelperClient> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PDF_CHROME_PDF_DOCUMENT_HELPER_CLIENT_H_
