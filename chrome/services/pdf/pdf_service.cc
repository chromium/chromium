// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/pdf/pdf_service.h"

#include <memory>
#include <utility>

#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/services/pdf/pdf_progressive_searchifier.h"
#include "chrome/services/pdf/pdf_searchifier.h"
#include "chrome/services/pdf/pdf_thumbnailer.h"
#include "chrome/services/pdf/public/mojom/pdf_service.mojom.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "content/public/child/child_thread.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace pdf {

PdfService::PdfService(mojo::PendingReceiver<mojom::PdfService> receiver)
    : receiver_(this, std::move(receiver)) {
  // Set up discardable memory for services that call into PDFium.
  //
  // When the PdfUseSkiaRenderer feature is on, PDFium requires Skia to render,
  // and Skia requires discardable memory. TODO(crbug.com/40061942): Clarify
  // this comment when PdfUseSkiaRenderer is on by default.
  mojo::PendingRemote<discardable_memory::mojom::DiscardableSharedMemoryManager>
      manager_remote;
  content::ChildThread::Get()->BindHostReceiver(
      manager_remote.InitWithNewPipeAndPassReceiver());
  discardable_shared_memory_manager_ = base::MakeRefCounted<
      discardable_memory::ClientDiscardableSharedMemoryManager>(
      std::move(manager_remote),
      content::UtilityThread::Get()->GetIOTaskRunner());
  base::DiscardableMemoryAllocator::SetInstance(
      discardable_shared_memory_manager_.get());
}

PdfService::~PdfService() = default;

void PdfService::BindPdfProgressiveSearchifier(
    mojo::PendingReceiver<mojom::PdfProgressiveSearchifier> receiver,
    mojo::PendingRemote<mojom::Ocr> ocr) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<pdf::PdfProgressiveSearchifier>(std::move(ocr)),
      std::move(receiver));
}

void PdfService::BindPdfSearchifier(
    mojo::PendingReceiver<mojom::PdfSearchifier> receiver,
    mojo::PendingRemote<mojom::Ocr> ocr) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<pdf::PdfSearchifier>(std::move(ocr)),
      std::move(receiver));
}

void PdfService::BindPdfThumbnailer(
    mojo::PendingReceiver<mojom::PdfThumbnailer> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<pdf::PdfThumbnailer>(),
                              std::move(receiver));
}

}  // namespace pdf
