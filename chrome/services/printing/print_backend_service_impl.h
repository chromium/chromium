// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_IMPL_H_
#define CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "printing/printing_context.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_WIN)
#include "base/types/expected.h"
#include "chrome/services/printing/public/mojom/printer_xml_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif  // BUILDFLAG(IS_WIN)

#if !BUILDFLAG(ENABLE_OOP_PRINTING)
#error "Out-of-process printing must be enabled."
#endif

namespace crash_keys {
class ScopedPrinterInfo;
}

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace printing {

class SandboxedPrintBackendHostImpl : public mojom::SandboxedPrintBackendHost {
 public:
  explicit SandboxedPrintBackendHostImpl(
      mojo::PendingReceiver<mojom::SandboxedPrintBackendHost> receiver);
  SandboxedPrintBackendHostImpl(const SandboxedPrintBackendHostImpl&) = delete;
  SandboxedPrintBackendHostImpl& operator=(
      const SandboxedPrintBackendHostImpl&) = delete;
  ~SandboxedPrintBackendHostImpl() override;

 private:
  // mojom::SandboxedPrintBackendHost
  void BindBackend(
      mojo::PendingReceiver<mojom::PrintBackendService> receiver) override;

  mojo::Receiver<mojom::SandboxedPrintBackendHost> receiver_;
  std::unique_ptr<mojom::PrintBackendService> print_backend_service_;
};

class UnsandboxedPrintBackendHostImpl
    : public mojom::UnsandboxedPrintBackendHost {
 public:
  explicit UnsandboxedPrintBackendHostImpl(
      mojo::PendingReceiver<mojom::UnsandboxedPrintBackendHost> receiver);
  UnsandboxedPrintBackendHostImpl(const UnsandboxedPrintBackendHostImpl&) =
      delete;
  UnsandboxedPrintBackendHostImpl& operator=(
      const UnsandboxedPrintBackendHostImpl&) = delete;
  ~UnsandboxedPrintBackendHostImpl() override;

 private:
  // mojom::UnsandboxedPrintBackendHost
  void BindBackend(
      mojo::PendingReceiver<mojom::PrintBackendService> receiver) override;

  mojo::Receiver<mojom::UnsandboxedPrintBackendHost> receiver_;
  std::unique_ptr<mojom::PrintBackendService> print_backend_service_;
};

class PrintBackendServiceImpl : public mojom::PrintBackendService {
 public:
  struct StartPrintingResult {
    mojom::ResultCode result;
    int job_id;
  };

  explicit PrintBackendServiceImpl(
      mojo::PendingReceiver<mojom::PrintBackendService> receiver);
  PrintBackendServiceImpl(const PrintBackendServiceImpl&) = delete;
  PrintBackendServiceImpl& operator=(const PrintBackendServiceImpl&) = delete;
  ~PrintBackendServiceImpl() override;

 protected:
  // Common initialization for both production and test instances.
  void InitCommon(
#if BUILDFLAG(IS_WIN)
      const std::string& locale,
      mojo::PendingRemote<mojom::PrinterXmlParser> remote
#else
      const std::string& locale
#endif  // BUILDFLAG(IS_WIN)
  );

 private:
  friend class PrintBackendServiceTestImpl;

  class DocumentHelper;
  struct ContextContainer;

  class PrintingContextDelegate : public PrintingContext::Delegate {
   public:
    PrintingContextDelegate();
    PrintingContextDelegate(const PrintingContextDelegate&) = delete;
    PrintingContextDelegate& operator=(const PrintingContextDelegate&) = delete;
    ~PrintingContextDelegate() override;

    // PrintingContext::Delegate overrides:
    gfx::NativeView GetParentView() override;
    std::string GetAppLocale() override;

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
    void SetParentWindow(uint32_t parent_window_id);
#endif
    void SetAppLocale(const std::string& locale);

   private:
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
    gfx::NativeView parent_native_view_ = gfx::NativeView();
#endif
    std::string locale_;
  };

  // mojom::PrintBackendService implementation:
  void Init(
#if BUILDFLAG(IS_WIN)
      const std::string& locale,
      mojo::PendingRemote<mojom::PrinterXmlParser> remote
#else
      const std::string& locale
#endif  // BUILDFLAG(IS_WIN)
      ) override;
  void Poke() override;
  void EnumeratePrinters(
      mojom::PrintBackendService::EnumeratePrintersCallback callback) override;
  void GetDefaultPrinterName(
      mojom::PrintBackendService::GetDefaultPrinterNameCallback callback)
      override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      mojom::PrintBackendService::GetPrinterSemanticCapsAndDefaultsCallback
          callback) override;
#endif
  void FetchCapabilities(
      const std::string& printer_name,
      mojom::PrintBackendService::FetchCapabilitiesCallback callback) override;
#if BUILDFLAG(IS_WIN)
  void GetPaperPrintableArea(
      const std::string& printer_name,
      const PrintSettings::RequestedMedia& media,
      mojom::PrintBackendService::GetPaperPrintableAreaCallback callback)
      override;
#endif
  void EstablishPrintingContext(uint32_t context_id
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
                                ,
                                uint32_t parent_window_id
#endif
                                ) override;
  void UseDefaultSettings(
      uint32_t context_id,
      mojom::PrintBackendService::UseDefaultSettingsCallback callback) override;
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  void AskUserForSettings(
      uint32_t context_id,
      int max_pages,
      bool has_selection,
      bool is_scripted,
      mojom::PrintBackendService::AskUserForSettingsCallback callback) override;
#endif
  void UpdatePrintSettings(
      uint32_t context_id,
      base::Value::Dict job_settings,
      mojom::PrintBackendService::UpdatePrintSettingsCallback callback)
      override;
  void StartPrinting(
      uint32_t context_id,
      int document_cookie,
      const std::u16string& document_name,
#if !BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
      const std::optional<PrintSettings>& settings,
#endif
      mojom::PrintBackendService::StartPrintingCallback callback) override;
#if BUILDFLAG(IS_WIN)
  void RenderPrintedPage(
      int32_t document_cookie,
      uint32_t page_index,
      mojom::MetafileDataType page_data_type,
      base::ReadOnlySharedMemoryRegion serialized_page,
      const gfx::Size& page_size,
      const gfx::Rect& page_content_rect,
      float shrink_factor,
      mojom::PrintBackendService::RenderPrintedPageCallback callback) override;
#endif  // BUILDFLAG(IS_WIN)
  void RenderPrintedDocument(
      int32_t document_cookie,
      uint32_t page_count,
      mojom::MetafileDataType data_type,
      base::ReadOnlySharedMemoryRegion serialized_doc,
      mojom::PrintBackendService::RenderPrintedDocumentCallback callback)
      override;
  void DocumentDone(
      int32_t document_cookie,
      mojom::PrintBackendService::DocumentDoneCallback callback) override;
  void Cancel(int32_t document_cookie,
              mojom::PrintBackendService::CancelCallback callback) override;

  // Callbacks from worker functions.
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  void OnDidAskUserForSettings(
      uint32_t context_id,
      mojom::PrintBackendService::AskUserForSettingsCallback callback,
      mojom::ResultCode result);
#endif
  void OnDidStartPrintingReadyDocument(DocumentHelper& document_helper,
                                       StartPrintingResult printing_result);
  void OnDidDocumentDone(
      DocumentHelper& document_helper,
      mojom::PrintBackendService::DocumentDoneCallback callback,
      mojom::ResultCode result);
  void OnDidCancel(DocumentHelper& document_helper,
                   mojom::PrintBackendService::CancelCallback callback);

  // Utility helpers.
  std::unique_ptr<PrintingContextDelegate> CreatePrintingContextDelegate();
  PrintingContext* GetPrintingContext(uint32_t context_id);
  DocumentHelper* GetDocumentHelper(int document_cookie);
  void RemoveDocumentHelper(DocumentHelper& document_helper);

#if BUILDFLAG(IS_WIN)
  // Get XPS capabilities for printer `printer_name`, or return
  // mojom::ResultCode on error.
  base::expected<XpsCapabilities, mojom::ResultCode> GetXpsCapabilities(
      const std::string& printer_name);
#endif  // BUILDFLAG(IS_WIN)

  // The locale provided at initialization that should be used with all
  // PrintingContext::Delegate instances.
  std::string locale_;

  // Crash key is kept at class level so that we can obtain printer driver
  // information for a prior call should the process be terminated by the
  // remote.  This can happen in the case of Mojo message validation.
  std::unique_ptr<crash_keys::ScopedPrinterInfo> crash_keys_;

  scoped_refptr<PrintBackend> print_backend_;

  // Map from a context ID to a printing device context.  Accessed only from
  // the main thread.
  base::flat_map<uint32_t, std::unique_ptr<ContextContainer>>
      persistent_printing_contexts_;

  // Want all callbacks and document helper sequence manipulations to be made
  // from main thread, not a thread runner.
  SEQUENCE_CHECKER(main_sequence_checker_);

  // Sequence of documents to be printed, in the order received.  Documents
  // could be removed from the list in any order, depending upon the speed
  // with which concurrent printing jobs are able to complete.  The
  // `DocumentHelper` objects are used and accessed only on the main thread.
  std::vector<std::unique_ptr<DocumentHelper>> documents_;

  mojo::Receiver<mojom::PrintBackendService> receiver_;

#if BUILDFLAG(IS_WIN)
  mojo::Remote<mojom::PrinterXmlParser> xml_parser_remote_;
#endif  // BUILDFLAG(IS_WIN)
};

}  // namespace printing

#endif  // CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_IMPL_H_
