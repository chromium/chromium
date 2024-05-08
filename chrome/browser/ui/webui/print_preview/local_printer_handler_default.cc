// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_default.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/common/printing/printer_capabilities_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/threading/thread_restrictions.h"
#endif

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/oop_features.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#endif

namespace printing {

namespace {

scoped_refptr<base::TaskRunner> CreatePrinterHandlerTaskRunner() {
  // USER_VISIBLE because the result is displayed in the print preview dialog.
#if !BUILDFLAG(IS_WIN)
  static constexpr base::TaskTraits kTraits = {
      base::MayBlock(), base::TaskPriority::USER_VISIBLE};
#endif

#if BUILDFLAG(USE_CUPS)
  // CUPS is thread safe.
  return base::ThreadPool::CreateTaskRunner(kTraits);
#elif BUILDFLAG(IS_WIN)
  // Windows drivers are likely not thread-safe and need to be accessed on the
  // UI thread.
  return content::GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE});
#else
  // Be conservative on unsupported platforms.
  return base::ThreadPool::CreateSingleThreadTaskRunner(kTraits);
#endif
}

}  // namespace

// static
PrinterList LocalPrinterHandlerDefault::EnumeratePrintersOnBlockingTaskRunner(
    const std::string& locale) {
#if BUILDFLAG(IS_WIN)
  // Blocking is needed here because Windows printer drivers are oftentimes
  // not thread-safe and have to be accessed on the UI thread.
  base::ScopedAllowBlocking allow_blocking;
#endif

  auto query_start_time = base::TimeTicks::Now();

  scoped_refptr<PrintBackend> print_backend(
      PrintBackend::CreateInstance(locale));

  PrinterList printer_list;
  mojom::ResultCode result = print_backend->EnumeratePrinters(printer_list);
  base::UmaHistogramTimes("PrintPreview.EnumeratePrintersTime",
                          base::TimeTicks::Now() - query_start_time);
  if (result == mojom::ResultCode::kSuccess) {
    PRINTER_LOG(EVENT) << "Enumerated " << printer_list.size() << " printer(s)";
  } else {
    PRINTER_LOG(ERROR) << "Failure enumerating local printers, result: "
                       << result;
  }
  return printer_list;
}

// static
base::Value::Dict
LocalPrinterHandlerDefault::FetchCapabilitiesOnBlockingTaskRunner(
    const std::string& device_name,
    const std::string& locale) {
  PrinterSemanticCapsAndDefaults::Papers user_defined_papers;
#if BUILDFLAG(IS_MAC)
  user_defined_papers = GetMacCustomPaperSizes();
#endif

#if BUILDFLAG(IS_WIN)
  // Blocking is needed here because Windows printer drivers are oftentimes
  // not thread-safe and have to be accessed on the UI thread.
  base::ScopedAllowBlocking allow_blocking;
#endif

  auto query_start_time = base::TimeTicks::Now();

  scoped_refptr<PrintBackend> print_backend(
      PrintBackend::CreateInstance(locale));

  PrinterBasicInfo basic_info;
  mojom::ResultCode result =
      print_backend->GetPrinterBasicInfo(device_name, &basic_info);
  base::UmaHistogramTimes("PrintPreview.FetchCapabilitiesTime",
                          base::TimeTicks::Now() - query_start_time);
  if (result == mojom::ResultCode::kSuccess) {
    PRINTER_LOG(EVENT) << "Got basic info for " << device_name;
  } else {
    PRINTER_LOG(ERROR) << "Invalid printer when getting basic info for "
                       << device_name << ", result: " << result;
    return base::Value::Dict();
  }

  return GetSettingsOnBlockingTaskRunner(
      device_name, basic_info, std::move(user_defined_papers), print_backend);
}

// static
std::string LocalPrinterHandlerDefault::GetDefaultPrinterOnBlockingTaskRunner(
    const std::string& locale) {
#if BUILDFLAG(IS_WIN)
  // Blocking is needed here because Windows printer drivers are oftentimes
  // not thread-safe and have to be accessed on the UI thread.
  base::ScopedAllowBlocking allow_blocking;
#endif

  auto query_start_time = base::TimeTicks::Now();

  scoped_refptr<PrintBackend> print_backend(
      PrintBackend::CreateInstance(locale));

  std::string default_printer;
  mojom::ResultCode result =
      print_backend->GetDefaultPrinterName(default_printer);
  base::UmaHistogramTimes("PrintPreview.GetDefaultPrinterNameTime",
                          base::TimeTicks::Now() - query_start_time);
  if (result != mojom::ResultCode::kSuccess) {
    PRINTER_LOG(ERROR) << "Failure getting default printer name, result: "
                       << result;
    return std::string();
  }
  PRINTER_LOG(EVENT) << "Default Printer: " << default_printer;
  return default_printer;
}

LocalPrinterHandlerDefault::LocalPrinterHandlerDefault(
    content::WebContents* preview_web_contents)
    : preview_web_contents_(preview_web_contents),
      task_runner_(CreatePrinterHandlerTaskRunner()) {}

LocalPrinterHandlerDefault::~LocalPrinterHandlerDefault() = default;

void LocalPrinterHandlerDefault::Reset() {}

void LocalPrinterHandlerDefault::GetDefaultPrinter(DefaultPrinterCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (IsOopPrintingEnabled()) {
    PRINTER_LOG(EVENT) << "Getting default printer via service";
    auto query_start_time = base::TimeTicks::Now();
    PrintBackendServiceManager& service_mgr =
        PrintBackendServiceManager::GetInstance();
    service_mgr.GetDefaultPrinterName(base::BindOnce(
        &LocalPrinterHandlerDefault::
            OnDidGetDefaultPrinterNameFromPrintBackendService,
        weak_ptr_factory_.GetWeakPtr(), query_start_time, std::move(cb)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  PRINTER_LOG(EVENT) << "Getting default printer in-process";
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetDefaultPrinterOnBlockingTaskRunner,
                     g_browser_process->GetApplicationLocale()),
      std::move(cb));
}

void LocalPrinterHandlerDefault::StartGetPrinters(
    AddedPrintersCallback callback,
    GetPrintersDoneCallback done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (IsOopPrintingEnabled()) {
    PRINTER_LOG(EVENT) << "Enumerate printers start via service";
    auto query_start_time = base::TimeTicks::Now();
    PrintBackendServiceManager& service_mgr =
        PrintBackendServiceManager::GetInstance();
    service_mgr.EnumeratePrinters(
        base::BindOnce(&LocalPrinterHandlerDefault::
                           OnDidEnumeratePrintersFromPrintBackendService,
                       weak_ptr_factory_.GetWeakPtr(), query_start_time,
                       std::move(callback), std::move(done_callback)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  PRINTER_LOG(EVENT) << "Enumerate printers start in-process";
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EnumeratePrintersOnBlockingTaskRunner,
                     g_browser_process->GetApplicationLocale()),
      base::BindOnce(&ConvertPrinterListForCallback, std::move(callback),
                     std::move(done_callback)));
}

void LocalPrinterHandlerDefault::StartGetCapability(
    const std::string& device_name,
    GetCapabilityCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (IsOopPrintingEnabled()) {
    PRINTER_LOG(EVENT) << "Getting printer capabilities via service for "
                       << device_name;
    auto query_start_time = base::TimeTicks::Now();
    PrintBackendServiceManager& service_mgr =
        PrintBackendServiceManager::GetInstance();
    service_mgr.FetchCapabilities(
        device_name,
        base::BindOnce(&LocalPrinterHandlerDefault::
                           OnDidFetchCapabilitiesFromPrintBackendService,
                       weak_ptr_factory_.GetWeakPtr(), device_name,
                       service_mgr.PrinterDriverFoundToRequireElevatedPrivilege(
                           device_name),
                       query_start_time, std::move(cb)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  PRINTER_LOG(EVENT) << "Getting printer capabilities in-process for "
                     << device_name;
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FetchCapabilitiesOnBlockingTaskRunner, device_name,
                     g_browser_process->GetApplicationLocale()),
      std::move(cb));
}

void LocalPrinterHandlerDefault::StartPrint(
    const std::u16string& job_title,
    base::Value::Dict settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    PrintCallback callback) {
  StartLocalPrint(std::move(settings), std::move(print_data),
                  preview_web_contents_, std::move(callback));
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)

void LocalPrinterHandlerDefault::
    OnDidGetDefaultPrinterNameFromPrintBackendService(
        base::TimeTicks query_start_time,
        DefaultPrinterCallback callback,
        mojom::DefaultPrinterNameResultPtr result) {
  base::UmaHistogramTimes("PrintPreview.GetDefaultPrinterNameTime",
                          base::TimeTicks::Now() - query_start_time);

  if (result->is_result_code()) {
    PRINTER_LOG(ERROR)
        << "Failure getting default printer via service, result: "
        << result->get_result_code();
    std::move(callback).Run(std::string());
    return;
  }

  PRINTER_LOG(EVENT) << "Default Printer from service: "
                     << result->get_default_printer_name();
  std::move(callback).Run(result->get_default_printer_name());
}

void LocalPrinterHandlerDefault::OnDidEnumeratePrintersFromPrintBackendService(
    base::TimeTicks query_start_time,
    AddedPrintersCallback added_printers_callback,
    GetPrintersDoneCallback done_callback,
    mojom::PrinterListResultPtr result) {
  base::UmaHistogramTimes("PrintPreview.EnumeratePrintersTime",
                          base::TimeTicks::Now() - query_start_time);

  PrinterList printer_list;
  if (result->is_printer_list()) {
    printer_list = std::move(result->get_printer_list());
    PRINTER_LOG(EVENT) << "Enumerated " << printer_list.size() << " printer(s)";
  } else {
    PRINTER_LOG(ERROR)
        << "Failure enumerating local printers via service, result: "
        << result->get_result_code();
  }

  ConvertPrinterListForCallback(std::move(added_printers_callback),
                                std::move(done_callback), printer_list);
}

void LocalPrinterHandlerDefault::OnDidFetchCapabilitiesFromPrintBackendService(
    const std::string& device_name,
    bool elevated_privileges,
    base::TimeTicks query_start_time,
    GetCapabilityCallback callback,
    mojom::PrinterCapsAndInfoResultPtr result) {
  base::UmaHistogramTimes("PrintPreview.FetchCapabilitiesTime",
                          base::TimeTicks::Now() - query_start_time);

  if (result->is_result_code()) {
    PRINTER_LOG(ERROR)
        << "Failure fetching printer capabilities via service for "
        << device_name << ", result: " << result->get_result_code();

    // If we failed because of access denied then we could retry at an elevated
    // privilege (if not already elevated).
    if (result->get_result_code() == mojom::ResultCode::kAccessDenied &&
        !elevated_privileges) {
      // Register that this printer requires elevated privileges.
      PrintBackendServiceManager& service_mgr =
          PrintBackendServiceManager::GetInstance();
      service_mgr.SetPrinterDriverFoundToRequireElevatedPrivilege(device_name);

      // Retry the operation which should now happen at a higher privilege
      // level.
      auto query_restart_time = base::TimeTicks::Now();
      service_mgr.FetchCapabilities(
          device_name,
          base::BindOnce(&LocalPrinterHandlerDefault::
                             OnDidFetchCapabilitiesFromPrintBackendService,
                         weak_ptr_factory_.GetWeakPtr(), device_name,
                         /*elevated_privileges=*/true, query_restart_time,
                         std::move(callback)));
      return;
    }

    // Unable to fallback, call back without data.
    std::move(callback).Run(base::Value::Dict());
    return;
  }

  PRINTER_LOG(EVENT) << "Received printer info & capabilities via service for "
                     << device_name;
  const mojom::PrinterCapsAndInfoPtr& caps_and_info =
      result->get_printer_caps_and_info();
  base::Value::Dict settings = AssemblePrinterSettings(
      device_name, caps_and_info->printer_info,
      /*has_secure_protocol=*/false, &caps_and_info->printer_caps);
  std::move(callback).Run(std::move(settings));
}

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

}  // namespace printing
