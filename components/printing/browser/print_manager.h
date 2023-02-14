// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_H_

#include <map>
#include <memory>

#include "build/build_config.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include <utility>

#include "base/functional/callback.h"
#endif

namespace printing {

class PrintManager : public content::WebContentsObserver,
                     public mojom::PrintManagerHost {
 public:
  PrintManager(const PrintManager&) = delete;
  PrintManager& operator=(const PrintManager&) = delete;
  ~PrintManager() override;

  void BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::PrintManagerHost> receiver,
      content::RenderFrameHost* rfh);

#if BUILDFLAG(IS_ANDROID)
  // TODO(timvolodine): consider introducing PrintManagerAndroid (crbug/500960)
  using PdfWritingDoneCallback =
      base::RepeatingCallback<void(int /* page count */)>;

  virtual void PdfWritingDone(int page_count) = 0;
#endif

  // printing::mojom::PrintManagerHost:
  void DidGetPrintedPagesCount(int32_t cookie, uint32_t number_pages) override;
  void DidPrintDocument(mojom::DidPrintDocumentParamsPtr params,
                        DidPrintDocumentCallback callback) override;
  void IsPrintingEnabled(IsPrintingEnabledCallback callback) override;
  void DidShowPrintDialog() override;
  void PrintingFailed(int32_t cookie,
                      mojom::PrintFailureReason reason) override;

  void ClearPrintRenderFramesForTesting();

 protected:
  explicit PrintManager(content::WebContents* contents);

  // Helper method to determine if PrintRenderFrame associated remote interface
  // is still connected.
  bool IsPrintRenderFrameConnected(content::RenderFrameHost* rfh) const;

  // Helper method to fetch the PrintRenderFrame associated remote interface
  // pointer.
  const mojo::AssociatedRemote<printing::mojom::PrintRenderFrame>&
  GetPrintRenderFrame(content::RenderFrameHost* rfh);

  // Returns the RenderFrameHost currently targeted by message dispatch.
  content::RenderFrameHost* GetCurrentTargetFrame();

  content::RenderFrameHostReceiverSet<printing::mojom::PrintManagerHost>&
  print_manager_host_receivers_for_testing() {
    return print_manager_host_receivers_;
  }

  // Terminates or cancels the print job if one was pending.
  void PrintingRenderFrameDeleted();

  bool IsValidCookie(int cookie) const;

  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  uint32_t number_pages() const { return number_pages_; }
  int cookie() const { return cookie_; }
  void set_cookie(int cookie) { cookie_ = cookie; }

#if BUILDFLAG(IS_ANDROID)
  PdfWritingDoneCallback pdf_writing_done_callback() const {
    return pdf_writing_done_callback_;
  }
  void set_pdf_writing_done_callback(PdfWritingDoneCallback callback) {
    pdf_writing_done_callback_ = std::move(callback);
  }
#endif

 private:
  uint32_t number_pages_ = 0;  // Number of pages to print in the print job.
  int cookie_ = 0;        // The current document cookie.

  // Holds RenderFrameHost-associated mojo receivers.
  content::RenderFrameHostReceiverSet<printing::mojom::PrintManagerHost>
      print_manager_host_receivers_;

#if BUILDFLAG(IS_ANDROID)
  // Callback to execute when done writing pdf.
  PdfWritingDoneCallback pdf_writing_done_callback_;
#endif

  // Stores a PrintRenderFrame associated remote with the RenderFrameHost used
  // to bind it. The PrintRenderFrame is used to transmit mojo interface method
  // calls to the associated receiver.
  std::map<content::RenderFrameHost*,
           mojo::AssociatedRemote<printing::mojom::PrintRenderFrame>>
      print_render_frames_;
};

}  // namespace printing

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_H_
