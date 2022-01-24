// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_manager.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace printing {

PrintManager::PrintManager(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      print_manager_host_receivers_(contents, this) {}

PrintManager::~PrintManager() = default;

void PrintManager::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::PrintManagerHost> receiver,
    content::RenderFrameHost* rfh) {
  print_manager_host_receivers_.Bind(rfh, std::move(receiver));
}

void PrintManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  print_render_frames_.erase(render_frame_host);
}

void PrintManager::DidGetPrintedPagesCount(int32_t cookie,
                                           uint32_t number_pages) {
  DCHECK_GT(cookie, 0);
  DCHECK_GT(number_pages, 0u);
  number_pages_ = number_pages;
}

void PrintManager::DidShowPrintDialog() {}

void PrintManager::DidPrintDocument(mojom::DidPrintDocumentParamsPtr params,
                                    DidPrintDocumentCallback callback) {
  std::move(callback).Run(false);
}

void PrintManager::ShowInvalidPrinterSettingsError() {}

void PrintManager::PrintingFailed(int32_t cookie) {
  // Note: Not redundant with cookie checks in the same method in other parts of
  // the class hierarchy.
  if (!IsValidCookie(cookie))
    return;

#if defined(OS_ANDROID)
  PdfWritingDone(0);
#endif
}

bool PrintManager::IsPrintRenderFrameConnected(
    content::RenderFrameHost* rfh) const {
  auto it = print_render_frames_.find(rfh);
  return it != print_render_frames_.end() && it->second.is_bound() &&
         it->second.is_connected();
}

const mojo::AssociatedRemote<printing::mojom::PrintRenderFrame>&
PrintManager::GetPrintRenderFrame(content::RenderFrameHost* rfh) {
  auto it = print_render_frames_.find(rfh);
  if (it == print_render_frames_.end()) {
    mojo::AssociatedRemote<printing::mojom::PrintRenderFrame> remote;
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&remote);
    it = print_render_frames_.insert({rfh, std::move(remote)}).first;
  } else if (it->second.is_bound() && !it->second.is_connected()) {
    // When print preview is closed, the remote is disconnected from the
    // receiver. Reset and bind the remote before using it again.
    it->second.reset();
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&it->second);
  }

  return it->second;
}

content::RenderFrameHost* PrintManager::GetCurrentTargetFrame() {
  return print_manager_host_receivers_.GetCurrentTargetFrame();
}

void PrintManager::PrintingRenderFrameDeleted() {
#if defined(OS_ANDROID)
  PdfWritingDone(0);
#endif
}

bool PrintManager::IsValidCookie(int cookie) const {
  return cookie > 0 && cookie == cookie_;
}

}  // namespace printing
