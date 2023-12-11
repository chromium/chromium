// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_FAKE_PRINT_RENDER_FRAME_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_FAKE_PRINT_RENDER_FRAME_H_

#include "base/values.h"
#include "components/printing/common/print.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace printing {

// This class implements a PrintRenderFrame that can be attached to an
// AssociatedInterfaceProvider to intercept requests from the browser for unit
// tests.
class FakePrintRenderFrame : public mojom::PrintRenderFrame {
 public:
  explicit FakePrintRenderFrame(blink::AssociatedInterfaceProvider* provider);
  ~FakePrintRenderFrame() override;

 private:
  // printing::mojom::PrintRenderFrame:
  void PrintRequestedPages() override;
  void PrintWithParams(mojom::PrintPagesParamsPtr params,
                       PrintWithParamsCallback callback) override;
  void PrintForSystemDialog() override;
  void SetPrintPreviewUI(
      mojo::PendingAssociatedRemote<mojom::PrintPreviewUI> preview) override;
  void InitiatePrintPreview(
#if BUILDFLAG(IS_CHROMEOS_ASH)
      mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer,
#endif
      bool has_selection) override;
  void PrintPreview(base::Value::Dict settings) override;
  void OnPrintPreviewDialogClosed() override;
  void PrintFrameContent(mojom::PrintFrameContentParamsPtr params,
                         PrintFrameContentCallback callback) override;
  void PrintingDone(bool success) override;
  void ConnectToPdfRenderer() override;
  void PrintNodeUnderContextMenu() override;

  void BindPrintRenderFrameReceiver(mojo::ScopedInterfaceEndpointHandle handle);

  mojo::AssociatedReceiver<mojom::PrintRenderFrame> receiver_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_FAKE_PRINT_RENDER_FRAME_H_
