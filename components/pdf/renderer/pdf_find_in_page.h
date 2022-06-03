// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_FIND_IN_PAGE_H_
#define COMPONENTS_PDF_RENDERER_PDF_FIND_IN_PAGE_H_

#include <stdint.h>

#include <memory>

#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "pdf/mojom/pdf.mojom.h"

namespace content {
class RenderFrame;
}

namespace pdf {

// Facilitates find-in-page IPCs from PDF renderer to PDF extension.
// Has the same lifetime as the RenderFrame it is associated with.
class PdfFindInPageFactory : public content::RenderFrameObserver,
                             public pdf::mojom::PdfFindInPageFactory {
 public:
  static void BindReceiver(
      int32_t routing_id,
      mojo::PendingAssociatedReceiver<pdf::mojom::PdfFindInPageFactory>
          receiver);

  PdfFindInPageFactory(const PdfFindInPageFactory&) = delete;
  PdfFindInPageFactory& operator=(const PdfFindInPageFactory&) = delete;

  // content::RenderFrameObserver:
  void OnDestruct() override;

  // pdf::mojom::PdfFindInPageFactory:
  void GetPdfFindInPage(GetPdfFindInPageCallback callback) override;

 private:
  class FindInPageImpl;

  // Self deleting.
  PdfFindInPageFactory(
      content::RenderFrame* render_frame,
      mojo::PendingAssociatedReceiver<pdf::mojom::PdfFindInPageFactory>
          receiver);
  ~PdfFindInPageFactory() override;

  std::unique_ptr<FindInPageImpl> find_in_page_;
  mojo::AssociatedReceiver<pdf::mojom::PdfFindInPageFactory> receiver_;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_FIND_IN_PAGE_H_
