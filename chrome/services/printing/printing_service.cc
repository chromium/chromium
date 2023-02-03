// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/printing_service.h"

#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/services/printing/pdf_nup_converter.h"
#include "chrome/services/printing/pdf_to_pwg_raster_converter.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/services/printing/pdf_flattener.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_WIN)
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"  // nogncheck
#include "content/public/child/child_thread.h"      // nogncheck
#include "content/public/utility/utility_thread.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/services/printing/pdf_thumbnailer.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/services/printing/pdf_to_emf_converter.h"
#include "chrome/services/printing/pdf_to_emf_converter_factory.h"
#endif

namespace printing {

PrintingService::PrintingService(
    mojo::PendingReceiver<mojom::PrintingService> receiver)
    : receiver_(this, std::move(receiver)) {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_WIN)
  // Set up discardable memory for printing and thumbnailer.
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
#endif
}

PrintingService::~PrintingService() = default;

void PrintingService::BindPdfNupConverter(
    mojo::PendingReceiver<mojom::PdfNupConverter> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<printing::PdfNupConverter>(),
                              std::move(receiver));
}

void PrintingService::BindPdfToPwgRasterConverter(
    mojo::PendingReceiver<mojom::PdfToPwgRasterConverter> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<printing::PdfToPwgRasterConverter>(),
      std::move(receiver));
}

#if BUILDFLAG(IS_CHROMEOS)
void PrintingService::BindPdfFlattener(
    mojo::PendingReceiver<mojom::PdfFlattener> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<printing::PdfFlattener>(),
                              std::move(receiver));
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
void PrintingService::BindPdfThumbnailer(
    mojo::PendingReceiver<mojom::PdfThumbnailer> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<printing::PdfThumbnailer>(),
                              std::move(receiver));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
void PrintingService::BindPdfToEmfConverterFactory(
    mojo::PendingReceiver<mojom::PdfToEmfConverterFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<printing::PdfToEmfConverterFactory>(),
      std::move(receiver));
}
#endif

}  // namespace printing
