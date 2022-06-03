// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/fake_print_render_frame.h"

#include <utility>

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

void FakePrintRenderFrame::PrintForSystemDialog() {}

void FakePrintRenderFrame::SetPrintPreviewUI(
    mojo::PendingAssociatedRemote<mojom::PrintPreviewUI> preview) {}

void FakePrintRenderFrame::InitiatePrintPreview(
    mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer,
    bool has_selection) {}

void FakePrintRenderFrame::PrintPreview(base::Value settings) {}

void FakePrintRenderFrame::OnPrintPreviewDialogClosed() {}

void FakePrintRenderFrame::PrintFrameContent(
    mojom::PrintFrameContentParamsPtr params,
    PrintFrameContentCallback callback) {}

void FakePrintRenderFrame::PrintingDone(bool success) {}

void FakePrintRenderFrame::SetPrintingEnabled(bool enabled) {}

void FakePrintRenderFrame::PrintNodeUnderContextMenu() {}

void FakePrintRenderFrame::BindPrintRenderFrameReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::PrintRenderFrame>(
      std::move(handle)));
}

}  // namespace printing
