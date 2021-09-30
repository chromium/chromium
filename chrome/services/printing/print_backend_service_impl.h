// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_IMPL_H_
#define CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "printing/printing_context.h"
#include "ui/gfx/native_widget_types.h"

#if !BUILDFLAG(ENABLE_OOP_PRINTING)
#error "Out-of-process printing must be enabled."
#endif

namespace crash_keys {
class ScopedPrinterInfo;
}

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
  explicit PrintBackendServiceImpl(
      mojo::PendingReceiver<mojom::PrintBackendService> receiver);
  PrintBackendServiceImpl(const PrintBackendServiceImpl&) = delete;
  PrintBackendServiceImpl& operator=(const PrintBackendServiceImpl&) = delete;
  ~PrintBackendServiceImpl() override;

 private:
  friend class PrintBackendServiceTestImpl;

  struct DocumentContainer;

  class PrintingContextDelegate : public PrintingContext::Delegate {
   public:
    PrintingContextDelegate();
    PrintingContextDelegate(const PrintingContextDelegate&) = delete;
    PrintingContextDelegate& operator=(const PrintingContextDelegate&) = delete;
    ~PrintingContextDelegate() override;

    // PrintingContext::Delegate overrides:
    gfx::NativeView GetParentView() override;
    std::string GetAppLocale() override;

    void SetAppLocale(const std::string& locale);

   private:
    std::string locale_;
  };

  // mojom::PrintBackendService implementation:
  void Init(const std::string& locale) override;
  void Poke() override;
  void EnumeratePrinters(
      mojom::PrintBackendService::EnumeratePrintersCallback callback) override;
  void GetDefaultPrinterName(
      mojom::PrintBackendService::GetDefaultPrinterNameCallback callback)
      override;
  void GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      mojom::PrintBackendService::GetPrinterSemanticCapsAndDefaultsCallback
          callback) override;
  void FetchCapabilities(
      const std::string& printer_name,
      mojom::PrintBackendService::FetchCapabilitiesCallback callback) override;
  void UpdatePrintSettings(
      base::flat_map<std::string, base::Value> job_settings,
      mojom::PrintBackendService::UpdatePrintSettingsCallback callback)
      override;
  void StartPrinting(
      int document_cookie,
      const std::u16string& document_name,
      mojom::PrintTargetType target_type,
      int page_count,
      const PrintSettings& settings,
      mojom::PrintBackendService::StartPrintingCallback callback) override;

  // Helper function that runs on a task runner.
  mojom::ResultCode StartPrintingReadyDocument(
      PrintBackendServiceImpl::DocumentContainer& document_container);

  // Callback from helper function.
  void OnDidStartPrintingReadyDocument(
      PrintBackendServiceImpl::DocumentContainer& document_container,
      mojom::ResultCode result);

  // Crash key is kept at class level so that we can obtain printer driver
  // information for a prior call should the process be terminated by the
  // remote.  This can happen in the case of Mojo message validation.
  std::unique_ptr<crash_keys::ScopedPrinterInfo> crash_keys_;

  scoped_refptr<PrintBackend> print_backend_;

  PrintingContextDelegate context_delegate_;

  // Want all callbacks to be made from common thread, not a thread runner.
  SEQUENCE_CHECKER(callback_sequence_checker_);

  // Sequence of documents to be printed, in the order received.  Documents
  // could be removed from the list in any order, depending upon the speed
  // with which concurrent printing jobs are able to complete.
  std::vector<std::unique_ptr<DocumentContainer>> documents_;

  mojo::Receiver<mojom::PrintBackendService> receiver_;
};

}  // namespace printing

#endif  // CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_IMPL_H_
