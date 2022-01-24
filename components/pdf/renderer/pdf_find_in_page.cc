// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_find_in_page.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "pdf/pdf_features.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"

namespace {

blink::WebElement FindPdfViewerScroller(const blink::WebLocalFrame* frame) {
  blink::WebElement viewer = frame->GetDocument().GetElementById("viewer");
  if (viewer.IsNull())
    return blink::WebElement();

  blink::WebNode shadow_root = viewer.ShadowRoot();
  if (shadow_root.IsNull())
    return blink::WebElement();

  blink::WebElement plugin = shadow_root.QuerySelector("#plugin");
  if (plugin.IsNull() || !plugin.HasAttribute("pdf-viewer-update-enabled"))
    return blink::WebElement();

  return shadow_root.QuerySelector("#scroller");
}

}  // namespace

namespace pdf {

// static
void PdfFindInPageFactory::BindReceiver(
    int32_t routing_id,
    mojo::PendingAssociatedReceiver<pdf::mojom::PdfFindInPageFactory>
        receiver) {
  DCHECK(base::FeatureList::IsEnabled(chrome_pdf::features::kPdfUnseasoned));

  auto* render_frame = content::RenderFrame::FromRoutingID(routing_id);
  if (!render_frame)
    return;

  // PdfFindInPageFactory is self deleting.
  new PdfFindInPageFactory(render_frame, std::move(receiver));
}

void PdfFindInPageFactory::OnDestruct() {
  delete this;
}

void PdfFindInPageFactory::GetPdfFindInPage(GetPdfFindInPageCallback callback) {
  mojo::PendingReceiver<pdf::mojom::PdfFindInPage> pending_receiver;
  auto pending_remote = pending_receiver.InitWithNewPipeAndPassRemote();
  find_in_page_ = std::make_unique<FindInPageImpl>(render_frame(),
                                                   std::move(pending_receiver));
  std::move(callback).Run(std::move(pending_remote));
}

PdfFindInPageFactory::PdfFindInPageFactory(
    content::RenderFrame* render_frame,
    mojo::PendingAssociatedReceiver<pdf::mojom::PdfFindInPageFactory> receiver)
    : content::RenderFrameObserver(render_frame),
      receiver_(this, std::move(receiver)) {}

PdfFindInPageFactory::~PdfFindInPageFactory() = default;

class PdfFindInPageFactory::FindInPageImpl : public pdf::mojom::PdfFindInPage {
 public:
  FindInPageImpl(
      content::RenderFrame* render_frame,
      mojo::PendingReceiver<pdf::mojom::PdfFindInPage> pending_receiver)
      : render_frame_(render_frame),
        receiver_(this, std::move(pending_receiver)) {}

  FindInPageImpl(const FindInPageImpl&) = delete;
  FindInPageImpl& operator=(const FindInPageImpl&) = delete;

  ~FindInPageImpl() override = default;

  // pdf::mojom::PdfFindInPage:
  void SetTickmarks(const std::vector<gfx::Rect>& tickmarks) override {
    blink::WebVector<gfx::Rect> tickmarks_converted(tickmarks);
    blink::WebLocalFrame* frame = render_frame_->GetWebFrame();
    blink::WebElement target = FindPdfViewerScroller(frame);
    frame->SetTickmarks(target, tickmarks_converted);
  }

 private:
  content::RenderFrame* const render_frame_;
  mojo::Receiver<pdf::mojom::PdfFindInPage> receiver_;
};

}  // namespace pdf
