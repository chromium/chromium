// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/fake_print_render_frame.h"

#include <utility>

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace printing {

FakePrintRenderFrame::FakePrintRenderFrame(
    blink::AssociatedInterfaceProvider* provider) {
  provider->OverrideBinderForTesting(
      mojom::PrintRenderFrame::Name_,
      base::BindRepeating(&FakePrintRenderFrame::BindPrintRenderFrameReceiver,
                          base::Unretained(this)));
}

FakePrintRenderFrame::~FakePrintRenderFrame() = default;

void FakePrintRenderFrame::PrintRequestedPages() {}

void FakePrintRenderFrame::PrintWithParams(mojom::PrintPagesParamsPtr params,
                                           PrintWithParamsCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakePrintRenderFrame::PrintForSystemDialog() {}

void FakePrintRenderFrame::SetPrintPreviewUI(
    mojo::PendingAssociatedRemote<mojom::PrintPreviewUI> preview) {}

void FakePrintRenderFrame::InitiatePrintPreview(
#if BUILDFLAG(IS_CHROMEOS_ASH)
    mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer,
#endif
    bool has_selection) {
}

void FakePrintRenderFrame::PrintPreview(base::Value::Dict settings) {}

void FakePrintRenderFrame::OnPrintPreviewDialogClosed() {}

void FakePrintRenderFrame::PrintFrameContent(
    mojom::PrintFrameContentParamsPtr params,
    PrintFrameContentCallback callback) {}

void FakePrintRenderFrame::PrintingDone(bool success) {}

void FakePrintRenderFrame::ConnectToPdfRenderer() {}

void FakePrintRenderFrame::PrintNodeUnderContextMenu() {}

void FakePrintRenderFrame::BindPrintRenderFrameReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::PrintRenderFrame>(
      std::move(handle)));
}

}  // namespace printing
