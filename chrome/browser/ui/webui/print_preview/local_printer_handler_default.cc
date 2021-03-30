// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_default.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_backend_service.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "printing/printing_features.h"

#if defined(OS_MAC)
#include "chrome/common/printing/printer_capabilities_mac.h"
#endif

#if defined(OS_WIN)
#include "base/threading/thread_restrictions.h"
#endif

namespace printing {

namespace {

scoped_refptr<base::TaskRunner> CreatePrinterHandlerTaskRunner() {
  // USER_VISIBLE because the result is displayed in the print preview dialog.
#if !defined(OS_WIN)
  static constexpr base::TaskTraits kTraits = {
      base::MayBlock(), base::TaskPriority::USER_VISIBLE};
#endif

#if defined(USE_CUPS)
  // CUPS is thread safe.
  return base::ThreadPool::CreateTaskRunner(kTraits);
#elif defined(OS_WIN)
  // Windows drivers are likely not thread-safe and need to be accessed on the
  // UI thread.
  return content::GetUIThreadTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
#else
  // Be conservative on unsupported platforms.
  return base::ThreadPool::CreateSingleThreadTaskRunner(kTraits);
#endif
}

void OnDidGetDefaultPrinterName(
    PrinterHandler::DefaultPrinterCallback callback,
    const base::Optional<std::string>& printer_name) {
  if (!printer_name.has_value()) {
    LOG(WARNING) << "Failure getting default printer";
    std::move(callback).Run(std::string());
    return;
  }

  VLOG(1) << "Default Printer: " << printer_name.value();
  std::move(callback).Run(printer_name.value());
}

void OnDidEnumeratePrinters(
    PrinterHandler::AddedPrintersCallback added_printers_callback,
    PrinterHandler::GetPrintersDoneCallback done_callback,
    const base::Optional<PrinterList>& printer_list) {
  const bool have_printers = printer_list.has_value();
  if (!have_printers)
    LOG(WARNING) << "Failure enumerating local printers.";

  ConvertPrinterListForCallback(
      std::move(added_printers_callback), std::move(done_callback),
      have_printers ? printer_list.value() : PrinterList());
}

void OnDidFetchCapabilities(
    const std::string& device_name,
    bool has_secure_protocol,
    PrinterHandler::GetCapabilityCallback callback,
    const base::Optional<PrinterBasicInfo>& printer_info,
    const base::Optional<PrinterSemanticCapsAndDefaults::Papers>&
        user_defined_papers,
    const base::Optional<PrinterSemanticCapsAndDefaults>& caps_and_defaults) {
  const bool has_values = printer_info.has_value() &&
                          user_defined_papers.has_value() &&
                          caps_and_defaults.has_value();
  if (!has_values) {
    LOG(WARNING) << "Failure fetching printer capabilities for  "
                 << device_name;
    std::move(callback).Run(base::Value());
    return;
  }

  VLOG(1) << "Received printer info & capabilities for " << device_name;
  PrinterSemanticCapsAndDefaults caps = caps_and_defaults.value();
  base::Value settings = AssemblePrinterSettings(
      device_name, std::move(printer_info.value()),
      std::move(user_defined_papers.value()), has_secure_protocol, &caps);
  std::move(callback).Run(std::move(settings));
}

}  // namespace

// static
PrinterList LocalPrinterHandlerDefault::EnumeratePrintersAsync(
    const std::string& locale) {
#if defined(OS_WIN)
  // Blocking is needed here because Windows printer drivers are oftentimes
  // not thread-safe and have to be accessed on the UI thread.
  base::ScopedAllowBlocking allow_blocking;
#endif

  scoped_refptr<PrintBackend> print_backend(
      PrintBackend::CreateInstance(locale));

  PrinterList printer_list;
  print_backend->EnumeratePrinters(&printer_list);
  return printer_list;
}

// static
base::Value LocalPrinterHandlerDefault::FetchCapabilitiesAsync(
    const std::string& device_name,
    const std::string& locale) {
  PrinterSemanticCapsAndDefaults::Papers user_defined_papers;
#if defined(OS_MAC)
  user_defined_papers = GetMacCustomPaperSizes();
#endif

#if defined(OS_WIN)
  // Blocking is needed here because Windows printer drivers are oftentimes
  // not thread-safe and have to be accessed on the UI thread.
  base::ScopedAllowBlocking allow_blocking;
#endif

  scoped_refptr<PrintBackend> print_backend(
      PrintBackend::CreateInstance(locale));

  VLOG(1) << "Get printer capabilities start for " << device_name;

  PrinterBasicInfo basic_info;
  if (!print_backend->GetPrinterBasicInfo(device_name, &basic_info)) {
    LOG(WARNING) << "Invalid printer " << device_name;
    return base::Value();
  }

  return GetSettingsOnBlockingTaskRunner(
      device_name, basic_info, std::move(user_defined_papers),
      /*has_secure_protocol=*/false, print_backend);
}

// static
std::string LocalPrinterHandlerDefault::GetDefaultPrinterAsync(
    const std::string& locale) {
#if defined(OS_WIN)
  // Blocking is needed here because Windows printer drivers are oftentimes
  // not thread-safe and have to be accessed on the UI thread.
  base::ScopedAllowBlocking allow_blocking;
#endif

  scoped_refptr<PrintBackend> print_backend(
      PrintBackend::CreateInstance(locale));

  std::string default_printer = print_backend->GetDefaultPrinterName();
  VLOG(1) << "Default Printer: " << default_printer;
  return default_printer;
}

LocalPrinterHandlerDefault::LocalPrinterHandlerDefault(
    content::WebContents* preview_web_contents)
    : preview_web_contents_(preview_web_contents),
      task_runner_(CreatePrinterHandlerTaskRunner()) {}

LocalPrinterHandlerDefault::~LocalPrinterHandlerDefault() {}

void LocalPrinterHandlerDefault::Reset() {}

void LocalPrinterHandlerDefault::GetDefaultPrinter(DefaultPrinterCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (base::FeatureList::IsEnabled(features::kEnableOopPrintDrivers)) {
    VLOG(1) << "Getting default printer via service";
    GetPrintBackendService(g_browser_process->GetApplicationLocale(),
                           /*printer_name=*/std::string())
        ->GetDefaultPrinterName(
            base::BindOnce(&OnDidGetDefaultPrinterName, std::move(cb)));
  } else {
    VLOG(1) << "Getting default printer in-process";
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&GetDefaultPrinterAsync,
                       g_browser_process->GetApplicationLocale()),
        std::move(cb));
  }
}

void LocalPrinterHandlerDefault::StartGetPrinters(
    AddedPrintersCallback callback,
    GetPrintersDoneCallback done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (base::FeatureList::IsEnabled(features::kEnableOopPrintDrivers)) {
    VLOG(1) << "Enumerate printers start via service";
    GetPrintBackendService(g_browser_process->GetApplicationLocale(),
                           /*printer_name=*/std::string())
        ->EnumeratePrinters(base::BindOnce(&OnDidEnumeratePrinters,
                                           std::move(callback),
                                           std::move(done_callback)));
  } else {
    VLOG(1) << "Enumerate printers start in-process";
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&EnumeratePrintersAsync,
                       g_browser_process->GetApplicationLocale()),
        base::BindOnce(&ConvertPrinterListForCallback, std::move(callback),
                       std::move(done_callback)));
  }
}

void LocalPrinterHandlerDefault::StartGetCapability(
    const std::string& device_name,
    GetCapabilityCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (base::FeatureList::IsEnabled(features::kEnableOopPrintDrivers)) {
    VLOG(1) << "Getting printer capabilities via service for " << device_name;
    GetPrintBackendService(g_browser_process->GetApplicationLocale(),
                           device_name)
        ->FetchCapabilities(
            device_name,
            base::BindOnce(&OnDidFetchCapabilities, device_name,
                           /*has_secure_protocol=*/false, std::move(cb)));
  } else {
    VLOG(1) << "Getting printer capabilities in-process for " << device_name;
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&FetchCapabilitiesAsync, device_name,
                       g_browser_process->GetApplicationLocale()),
        std::move(cb));
  }
}

void LocalPrinterHandlerDefault::StartPrint(
    const std::u16string& job_title,
    base::Value settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    PrintCallback callback) {
  StartLocalPrint(std::move(settings), std::move(print_data),
                  preview_web_contents_, std::move(callback));
}

}  // namespace printing
