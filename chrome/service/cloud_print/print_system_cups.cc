// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/print_system.h"

#include <cups/cups.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/service/cloud_print/cloud_print_service_helpers.h"
#include "components/crash/core/common/crash_keys.h"
#include "printing/backend/cups_helper.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "url/gurl.h"

namespace {

// Print system config options.
const char kCUPSPrintServerURLs[] = "print_server_urls";
const char kCUPSUpdateTimeoutMs[] = "update_timeout_ms";
const char kCUPSNotifyDelete[] = "notify_delete";
const char kCUPSSupportedMimeTipes[] = "supported_mime_types";

// Default mime types supported by CUPS
// http://www.cups.org/articles.php?L205+TFAQ+Q
const char kCUPSDefaultSupportedTypes[] =
    "application/pdf,application/postscript,image/jpeg,image/png,image/gif";

// Time interval to check for printer's updates.
constexpr base::TimeDelta kCheckForPrinterUpdatesTime = base::Minutes(5);

// Job update timeout
constexpr base::TimeDelta kJobUpdateTimeout = base::Seconds(5);

// Job id for dry run (it should not affect CUPS job ids, since 0 job-id is
// invalid in CUPS.
const int kDryRunJobId = 0;

}  // namespace

namespace cloud_print {

struct PrintServerInfoCUPS {
  GURL url;
  scoped_refptr<printing::PrintBackend> backend;
  printing::PrinterList printers;
  // CapsMap cache PPD until the next update and give a fast access to it by
  // printer name. PPD request is relatively expensive and this should minimize
  // the number of requests.
  typedef std::map<std::string, printing::PrinterCapsAndDefaults> CapsMap;
  CapsMap caps_cache;
};

class PrintSystemCUPS : public PrintSystem {
 public:
  explicit PrintSystemCUPS(const base::DictionaryValue* print_system_settings);

  // PrintSystem implementation.
  PrintSystemResult Init() override;
  PrintSystem::PrintSystemResult EnumeratePrinters(
      printing::PrinterList* printer_list) override;
  void GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      PrinterCapsAndDefaultsCallback callback) override;
  bool IsValidPrinter(const std::string& printer_name) override;
  bool ValidatePrintTicket(const std::string& printer_name,
                           const std::string& print_ticket_data,
                           const std::string& print_ticket_mime_type) override;
  bool GetJobDetails(const std::string& printer_name,
                     PlatformJobId job_id,
                     PrintJobDetails* job_details) override;
  PrintSystem::PrintServerWatcher* CreatePrintServerWatcher() override;
  PrintSystem::PrinterWatcher* CreatePrinterWatcher(
      const std::string& printer_name) override;
  PrintSystem::JobSpooler* CreateJobSpooler() override;
  bool UseCddAndCjt() override;
  std::string GetSupportedMimeTypes() override;

  // Helper functions.
  PlatformJobId SpoolPrintJob(const std::string& print_ticket,
                              const base::FilePath& print_data_file_path,
                              const std::string& print_data_mime_type,
                              const std::string& printer_name,
                              const std::string& job_title,
                              const std::vector<std::string>& tags,
                              bool* dry_run);
  bool GetPrinterInfo(const std::string& printer_name,
                      printing::PrinterBasicInfo* info);
  bool ParsePrintTicket(const std::string& print_ticket,
                        std::map<std::string, std::string>* options);

  // Synchronous version of GetPrinterCapsAndDefaults.
  bool GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      printing::PrinterCapsAndDefaults* printer_info);

  base::TimeDelta GetUpdateTimeout() const {
    return update_timeout_;
  }

  bool NotifyDelete() const {
    // Notify about deleted printers only when we
    // fetched printers list without errors.
    return notify_delete_ && printer_enum_succeeded_;
  }

 private:
  ~PrintSystemCUPS() override {}

  // Following functions are wrappers around corresponding CUPS functions.
  // <functions>2()  are called when print server is specified, and plain
  // version in another case. There is an issue specifing CUPS_HTTP_DEFAULT
  // in the <functions>2(), it does not work in CUPS prior to 1.4.
  static int GetJobs(cups_job_t** jobs,
                     const GURL& url,
                     http_encryption_t encryption,
                     const char* name,
                     int myjobs,
                     int whichjobs);
  static int PrintFile(const GURL& url,
                       http_encryption_t encryption,
                       const char* name,
                       const char* filename,
                       const char* title,
                       int num_options,
                       cups_option_t* options);

  void InitPrintBackends(const base::DictionaryValue* print_system_settings);
  void AddPrintServer(const std::string& url);

  void UpdatePrinters();

  // Full name contains print server url:port and printer name. Short name
  // is the name of the printer in the CUPS server.
  static std::string MakeFullPrinterName(const GURL& url,
                                         const std::string& short_printer_name);
  PrintServerInfoCUPS* FindServerByFullName(
      const std::string& full_printer_name, std::string* short_printer_name);

  // Helper method to invoke a PrinterCapsAndDefaultsCallback.
  static void RunCapsCallback(
      PrinterCapsAndDefaultsCallback callback,
      bool succeeded,
      const std::string& printer_name,
      const printing::PrinterCapsAndDefaults& printer_info);

  // Contains information about all print servers and backends this proxy is
  // connected to.
  std::vector<PrintServerInfoCUPS> print_servers_;

  base::TimeDelta update_timeout_ = kCheckForPrinterUpdatesTime;
  bool initialized_ = false;
  bool printer_enum_succeeded_ = false;
  bool notify_delete_ = true;
  http_encryption_t cups_encryption_ = HTTP_ENCRYPT_NEVER;
  std::string supported_mime_types_ = kCUPSDefaultSupportedTypes;
};

class PrintServerWatcherCUPS
  : public PrintSystem::PrintServerWatcher {
 public:
  explicit PrintServerWatcherCUPS(PrintSystemCUPS* print_system)
      : print_system_(print_system) {}

  PrintServerWatcherCUPS(const PrintServerWatcherCUPS&) = delete;
  PrintServerWatcherCUPS& operator=(const PrintServerWatcherCUPS&) = delete;

  // PrintSystem::PrintServerWatcher implementation.
  bool StartWatching(
      PrintSystem::PrintServerWatcher::Delegate* delegate) override {
    delegate_ = delegate;
    printers_hash_ = GetPrintersHash();
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PrintServerWatcherCUPS::CheckForUpdates, this),
        print_system_->GetUpdateTimeout());
    return true;
  }

  bool StopWatching() override {
    delegate_ = nullptr;
    return true;
  }

  void CheckForUpdates() {
    if (!delegate_)
      return;  // Orphan call. We have been stopped already.

    VLOG(1) << "CP_CUPS: Checking for new printers";
    std::string new_hash = GetPrintersHash();
    if (printers_hash_ != new_hash) {
      printers_hash_ = new_hash;
      delegate_->OnPrinterAdded();
    }
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PrintServerWatcherCUPS::CheckForUpdates, this),
        print_system_->GetUpdateTimeout());
  }

 protected:
  ~PrintServerWatcherCUPS() override { StopWatching(); }

 private:
  std::string GetPrintersHash() {
    printing::PrinterList printer_list;
    print_system_->EnumeratePrinters(&printer_list);

    // Sort printer names.
    std::vector<std::string> printers;
    for (const auto& it : printer_list)
      printers.push_back(it.printer_name);
    std::sort(printers.begin(), printers.end());

    std::string to_hash;
    for (const auto& printer : printers)
      to_hash += printer;
    return base::MD5String(to_hash);
  }

  scoped_refptr<PrintSystemCUPS> print_system_;
  PrintSystem::PrintServerWatcher::Delegate* delegate_ = nullptr;
  std::string printers_hash_;
};

class PrinterWatcherCUPS
    : public PrintSystem::PrinterWatcher {
 public:
  PrinterWatcherCUPS(PrintSystemCUPS* print_system,
                     const std::string& printer_name)
      : printer_name_(printer_name),
        print_system_(print_system) {
  }

  PrinterWatcherCUPS(const PrinterWatcherCUPS&) = delete;
  PrinterWatcherCUPS& operator=(const PrinterWatcherCUPS&) = delete;

  // PrintSystem::PrinterWatcher implementation.
  bool StartWatching(PrintSystem::PrinterWatcher::Delegate* delegate) override {
    scoped_refptr<printing::PrintBackend> print_backend(
        printing::PrintBackend::CreateInstanceForCloudPrint(
            /*print_backend_settings=*/nullptr));
    crash_keys::ScopedPrinterInfo crash_key(
        print_backend->GetPrinterDriverInfo(printer_name_));
    if (delegate_)
      StopWatching();
    delegate_ = delegate;
    settings_hash_ = GetSettingsHash();
    // Schedule next job status update.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&PrinterWatcherCUPS::JobStatusUpdate, this),
        kJobUpdateTimeout);
    // Schedule next printer check.
    // TODO(gene): Randomize time for the next printer update.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&PrinterWatcherCUPS::PrinterUpdate, this),
        print_system_->GetUpdateTimeout());
    return true;
  }

  bool StopWatching() override {
    delegate_ = nullptr;
    return true;
  }

  bool GetCurrentPrinterInfo(
      printing::PrinterBasicInfo* printer_info) override {
    DCHECK(printer_info);
    return print_system_->GetPrinterInfo(printer_name_, printer_info);
  }

  void JobStatusUpdate() {
    if (!delegate_)
      return;  // Orphan call. We have been stopped already.

    // For CUPS proxy, we are going to fire OnJobChanged notification
    // periodically. Higher level will check if there are any outstanding
    // jobs for this printer and check their status. If printer has no
    // outstanding jobs, OnJobChanged() will do nothing.
    delegate_->OnJobChanged();
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&PrinterWatcherCUPS::JobStatusUpdate, this),
        kJobUpdateTimeout);
  }

  void PrinterUpdate() {
    if (!delegate_)
      return;  // Orphan call. We have been stopped already.

    VLOG(1) << "CP_CUPS: Checking for updates"
            << ", printer name: " << printer_name_;
    if (print_system_->NotifyDelete() &&
        !print_system_->IsValidPrinter(printer_name_)) {
      delegate_->OnPrinterDeleted();
      VLOG(1) << "CP_CUPS: Printer deleted"
              << ", printer name: " << printer_name_;
    } else {
      std::string new_hash = GetSettingsHash();
      if (settings_hash_ != new_hash) {
        settings_hash_ = new_hash;
        delegate_->OnPrinterChanged();
        VLOG(1) << "CP_CUPS: Printer configuration changed"
                << ", printer name: " << printer_name_;
      }
    }
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&PrinterWatcherCUPS::PrinterUpdate, this),
        print_system_->GetUpdateTimeout());
  }

 protected:
  ~PrinterWatcherCUPS() override { StopWatching(); }

 private:
  std::string GetSettingsHash() {
    printing::PrinterBasicInfo info;
    if (!print_system_->GetPrinterInfo(printer_name_, &info))
      return std::string();

    printing::PrinterCapsAndDefaults caps;
    if (!print_system_->GetPrinterCapsAndDefaults(printer_name_, &caps))
      return std::string();

    std::string to_hash(info.printer_name);
    to_hash += info.printer_description;
    for (const auto& it : info.options) {
      to_hash += it.first;
      to_hash += it.second;
    }

    to_hash += caps.printer_capabilities;
    to_hash += caps.caps_mime_type;
    to_hash += caps.printer_defaults;
    to_hash += caps.defaults_mime_type;

    return base::MD5String(to_hash);
  }
  const std::string printer_name_;
  PrintSystem::PrinterWatcher::Delegate* delegate_ = nullptr;
  scoped_refptr<PrintSystemCUPS> print_system_;
  std::string settings_hash_;
};

class JobSpoolerCUPS : public PrintSystem::JobSpooler {
 public:
  explicit JobSpoolerCUPS(PrintSystemCUPS* print_system)
      : print_system_(print_system) {
    DCHECK(print_system_.get());
  }

  JobSpoolerCUPS(const JobSpoolerCUPS&) = delete;
  JobSpoolerCUPS& operator=(const JobSpoolerCUPS&) = delete;

  // PrintSystem::JobSpooler implementation.
  bool Spool(const std::string& print_ticket,
             const std::string& print_ticket_mime_type,
             const base::FilePath& print_data_file_path,
             const std::string& print_data_mime_type,
             const std::string& printer_name,
             const std::string& job_title,
             const std::vector<std::string>& tags,
             JobSpooler::Delegate* delegate) override {
    DCHECK(delegate);
    bool dry_run = false;
    int job_id = print_system_->SpoolPrintJob(
        print_ticket, print_data_file_path, print_data_mime_type,
        printer_name, job_title, tags, &dry_run);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&JobSpoolerCUPS::NotifyDelegate, delegate,
                                  job_id, dry_run));
    return true;
  }

  static void NotifyDelegate(JobSpooler::Delegate* delegate,
                             int job_id, bool dry_run) {
    if (dry_run || job_id)
      delegate->OnJobSpoolSucceeded(job_id);
    else
      delegate->OnJobSpoolFailed();
  }

 protected:
  ~JobSpoolerCUPS() override {}

 private:
  scoped_refptr<PrintSystemCUPS> print_system_;
};

PrintSystemCUPS::PrintSystemCUPS(
    const base::DictionaryValue* print_system_settings) {
  if (print_system_settings) {
    int timeout;
    if (print_system_settings->GetInteger(kCUPSUpdateTimeoutMs, &timeout))
      update_timeout_ = base::Milliseconds(timeout);

    int encryption;
    if (print_system_settings->GetInteger(kCUPSEncryption, &encryption))
      cups_encryption_ = static_cast<http_encryption_t>(encryption);

    notify_delete_ = print_system_settings->FindBoolPath(kCUPSNotifyDelete)
                         .value_or(notify_delete_);

    std::string types;
    if (print_system_settings->GetString(kCUPSSupportedMimeTipes, &types))
      supported_mime_types_ = types;
  }

  InitPrintBackends(print_system_settings);
}

void PrintSystemCUPS::InitPrintBackends(
    const base::DictionaryValue* print_system_settings) {
  if (print_system_settings) {
    const base::Value* url_list =
        print_system_settings->FindListKey(kCUPSPrintServerURLs);
    if (url_list) {
      for (const base::Value& val : url_list->GetList()) {
        const std::string* print_server_url = val.GetIfString();
        if (print_server_url)
          AddPrintServer(*print_server_url);
      }
    }
  }

  // If server list is empty, use default print server.
  if (print_servers_.empty())
    AddPrintServer(std::string());
}

void PrintSystemCUPS::AddPrintServer(const std::string& url) {
  if (url.empty())
    LOG(WARNING) << "No print server specified. Using default print server.";

  // Get Print backend for the specific print server.
  base::DictionaryValue backend_settings;
  backend_settings.SetString(kCUPSPrintServerURL, url);

  // Make CUPS requests non-blocking.
  backend_settings.SetString(kCUPSBlocking, kValueFalse);

  // Set encryption for backend.
  backend_settings.SetInteger(kCUPSEncryption, cups_encryption_);

  PrintServerInfoCUPS print_server;
  print_server.backend =
      printing::PrintBackend::CreateInstanceForCloudPrint(&backend_settings);
  print_server.url = GURL(url.c_str());

  print_servers_.push_back(print_server);
}

PrintSystem::PrintSystemResult PrintSystemCUPS::Init() {
  UpdatePrinters();
  initialized_ = true;
  return PrintSystemResult(true, std::string());
}

void PrintSystemCUPS::UpdatePrinters() {
  printer_enum_succeeded_ = true;
  for (auto& print_server : print_servers_) {
    if (print_server.backend->EnumeratePrinters(&print_server.printers) !=
        printing::mojom::ResultCode::kSuccess)
      printer_enum_succeeded_ = false;
    print_server.caps_cache.clear();
    for (auto& printer : print_server.printers) {
      printer.printer_name =
          MakeFullPrinterName(print_server.url, printer.printer_name);
    }
    VLOG(1) << "CP_CUPS: Updated printers list"
            << ", server: " << print_server.url
            << ", # of printers: " << print_server.printers.size();
  }

  // Schedule next update.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&PrintSystemCUPS::UpdatePrinters, this),
      GetUpdateTimeout());
}

PrintSystem::PrintSystemResult PrintSystemCUPS::EnumeratePrinters(
    printing::PrinterList* printer_list) {
  DCHECK(initialized_);
  printer_list->clear();
  for (const auto& print_server : print_servers_) {
    printer_list->insert(printer_list->end(), print_server.printers.begin(),
                         print_server.printers.end());
  }
  VLOG(1) << "CP_CUPS: Total printers enumerated: " << printer_list->size();
  // TODO(sanjeevr): Maybe some day we want to report the actual server names
  // for which the enumeration failed.
  return PrintSystemResult(printer_enum_succeeded_, std::string());
}

void PrintSystemCUPS::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    PrinterCapsAndDefaultsCallback callback) {
  printing::PrinterCapsAndDefaults printer_info;
  bool succeeded = GetPrinterCapsAndDefaults(printer_name, &printer_info);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrintSystemCUPS::RunCapsCallback, std::move(callback),
                     succeeded, printer_name, printer_info));
}

bool PrintSystemCUPS::IsValidPrinter(const std::string& printer_name) {
  return GetPrinterInfo(printer_name, nullptr);
}

bool PrintSystemCUPS::ValidatePrintTicket(
    const std::string& printer_name,
    const std::string& print_ticket_data,
    const std::string& print_ticket_mime_type) {
  DCHECK(initialized_);
  absl::optional<base::Value> ticket =
      base::JSONReader::Read(print_ticket_data);
  return ticket.has_value() && ticket.value().is_dict();
}

// Print ticket on linux is a JSON string containing only one dictionary.
bool PrintSystemCUPS::ParsePrintTicket(
    const std::string& print_ticket,
    std::map<std::string, std::string>* options) {
  DCHECK(options);
  absl::optional<base::Value> ticket = base::JSONReader::Read(print_ticket);
  if (!ticket.has_value() || !ticket.value().is_dict())
    return false;

  options->clear();
  for (const auto it : ticket.value().DictItems()) {
    if (it.second.is_string())
      (*options)[it.first] = it.second.GetString();
  }
  return true;
}

bool PrintSystemCUPS::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    printing::PrinterCapsAndDefaults* printer_info) {
  DCHECK(initialized_);
  std::string short_printer_name;
  PrintServerInfoCUPS* server_info =
      FindServerByFullName(printer_name, &short_printer_name);
  if (!server_info)
    return false;

  PrintServerInfoCUPS::CapsMap::const_iterator caps_it =
      server_info->caps_cache.find(printer_name);
  if (caps_it != server_info->caps_cache.end()) {
    *printer_info = caps_it->second;
    return true;
  }

  // TODO(gene): Retry multiple times in case of error.
  crash_keys::ScopedPrinterInfo crash_key(
      server_info->backend->GetPrinterDriverInfo(short_printer_name));
  if (server_info->backend->GetPrinterCapsAndDefaults(short_printer_name,
                                                      printer_info) !=
      printing::mojom::ResultCode::kSuccess) {
    return false;
  }

  server_info->caps_cache[printer_name] = *printer_info;
  return true;
}

bool PrintSystemCUPS::GetJobDetails(const std::string& printer_name,
                                    PlatformJobId job_id,
                                    PrintJobDetails* job_details) {
  DCHECK(initialized_);
  DCHECK(job_details);

  std::string short_printer_name;
  const PrintServerInfoCUPS* server_info =
      FindServerByFullName(printer_name, &short_printer_name);
  if (!server_info)
    return false;

  crash_keys::ScopedPrinterInfo crash_key(
      server_info->backend->GetPrinterDriverInfo(short_printer_name));
  cups_job_t* jobs = nullptr;
  int num_jobs = GetJobs(&jobs, server_info->url, cups_encryption_,
                         short_printer_name.c_str(), 1, -1);
  bool error = (num_jobs == 0) && (cupsLastError() > IPP_OK_EVENTS_COMPLETE);
  if (error) {
    VLOG(1) << "CP_CUPS: Error getting jobs from CUPS server"
            << ", printer name:" << printer_name
            << ", error: " << static_cast<int>(cupsLastError());
    return false;
  }

  // Check if the request is for dummy dry run job.
  // We check this after calling GetJobs API to see if this printer is actually
  // accessible through CUPS.
  if (job_id == kDryRunJobId) {
    job_details->status = PRINT_JOB_STATUS_COMPLETED;
    VLOG(1) << "CP_CUPS: Dry run job succeeded"
            << ", printer name: " << printer_name;
    return true;
  }

  bool found = false;
  for (int i = 0; i < num_jobs; i++) {
    if (jobs[i].id == job_id) {
      found = true;
      switch (jobs[i].state) {
        case IPP_JOB_PENDING :
        case IPP_JOB_HELD :
        case IPP_JOB_PROCESSING :
          job_details->status = PRINT_JOB_STATUS_IN_PROGRESS;
          break;
        case IPP_JOB_STOPPED :
        case IPP_JOB_CANCELLED :
        case IPP_JOB_ABORTED :
          job_details->status = PRINT_JOB_STATUS_ERROR;
          break;
        case IPP_JOB_COMPLETED :
          job_details->status = PRINT_JOB_STATUS_COMPLETED;
          break;
        default:
          job_details->status = PRINT_JOB_STATUS_INVALID;
      }
      job_details->platform_status_flags = jobs[i].state;

      // We don't have any details on the number of processed pages here.
      break;
    }
  }

  if (found) {
    VLOG(1) << "CP_CUPS: Job found"
            << ", printer name: " << printer_name
            << ", cups job id: " << job_id
            << ", cups job status: " << job_details->status;
  } else {
    LOG(WARNING) << "CP_CUPS: Job not found"
                 << ", printer name: " << printer_name
                 << ", cups job id: " << job_id;
  }

  cupsFreeJobs(num_jobs, jobs);
  return found;
}

bool PrintSystemCUPS::GetPrinterInfo(const std::string& printer_name,
                                     printing::PrinterBasicInfo* info) {
  DCHECK(initialized_);
  if (info) {
    VLOG(1) << "CP_CUPS: Getting printer info"
            << ", printer name: " << printer_name;
  }

  std::string short_printer_name;
  const PrintServerInfoCUPS* server_info =
      FindServerByFullName(printer_name, &short_printer_name);
  if (!server_info)
    return false;

  for (const auto& printer : server_info->printers) {
    if (printer.printer_name == printer_name) {
      if (info)
        *info = printer;
      return true;
    }
  }
  return false;
}

PrintSystem::PrintServerWatcher* PrintSystemCUPS::CreatePrintServerWatcher() {
  DCHECK(initialized_);
  return new PrintServerWatcherCUPS(this);
}

PrintSystem::PrinterWatcher* PrintSystemCUPS::CreatePrinterWatcher(
    const std::string& printer_name) {
  DCHECK(initialized_);
  DCHECK(!printer_name.empty());
  return new PrinterWatcherCUPS(this, printer_name);
}

PrintSystem::JobSpooler* PrintSystemCUPS::CreateJobSpooler() {
  DCHECK(initialized_);
  return new JobSpoolerCUPS(this);
}

bool PrintSystemCUPS::UseCddAndCjt() {
  return false;
}

std::string PrintSystemCUPS::GetSupportedMimeTypes() {
  return supported_mime_types_;
}

scoped_refptr<PrintSystem> PrintSystem::CreateInstance(
    const base::DictionaryValue* print_system_settings) {
  return base::MakeRefCounted<PrintSystemCUPS>(print_system_settings);
}

// static
int PrintSystemCUPS::PrintFile(const GURL& url,
                               http_encryption_t encryption,
                               const char* name,
                               const char* filename,
                               const char* title,
                               int num_options,
                               cups_option_t* options) {
  // Use default (local) print server.
  if (url.is_empty())
    return cupsPrintFile(name, filename, title, num_options, options);

  printing::HttpConnectionCUPS http(url, encryption, /*blocking=*/false);
  return cupsPrintFile2(http.http(), name, filename, title, num_options,
                        options);
}

// static
int PrintSystemCUPS::GetJobs(cups_job_t** jobs,
                             const GURL& url,
                             http_encryption_t encryption,
                             const char* name,
                             int myjobs,
                             int whichjobs) {
  // Use default (local) print server.
  if (url.is_empty())
    return cupsGetJobs(jobs, name, myjobs, whichjobs);

  printing::HttpConnectionCUPS http(url, encryption, /*blocking=*/false);
  return cupsGetJobs2(http.http(), jobs, name, myjobs, whichjobs);
}

PlatformJobId PrintSystemCUPS::SpoolPrintJob(
    const std::string& print_ticket,
    const base::FilePath& print_data_file_path,
    const std::string& print_data_mime_type,
    const std::string& printer_name,
    const std::string& job_title,
    const std::vector<std::string>& tags,
    bool* dry_run) {
  DCHECK(initialized_);
  VLOG(1) << "CP_CUPS: Spooling print job, printer name: " << printer_name;

  std::string short_printer_name;
  const PrintServerInfoCUPS* server_info =
      FindServerByFullName(printer_name, &short_printer_name);
  if (!server_info)
    return false;

  crash_keys::ScopedPrinterInfo crash_key(
      server_info->backend->GetPrinterDriverInfo(printer_name));

  // We need to store options as char* string for the duration of the
  // cupsPrintFile2 call. We'll use map here to store options, since
  // Dictionary value from JSON parser returns wchat_t.
  std::map<std::string, std::string> options;
  bool res = ParsePrintTicket(print_ticket, &options);
  DCHECK(res);  // If print ticket is invalid we still print using defaults.

  // Check if this is a dry run (test) job.
  *dry_run = IsDryRunJob(tags);
  if (*dry_run) {
    VLOG(1) << "CP_CUPS: Dry run job spooled";
    return kDryRunJobId;
  }

  std::vector<cups_option_t> cups_options;
  for (const auto& it : options) {
    cups_option_t opt;
    opt.name = const_cast<char*>(it.first.c_str());
    opt.value = const_cast<char*>(it.second.c_str());
    cups_options.push_back(opt);
  }

  int job_id =
      PrintFile(server_info->url, cups_encryption_, short_printer_name.c_str(),
                print_data_file_path.value().c_str(), job_title.c_str(),
                cups_options.size(), cups_options.data());
  base::DeleteFile(print_data_file_path);

  // TODO(alexyu): Output printer id.
  VLOG(1) << "CP_CUPS: Job spooled"
          << ", printer name: " << printer_name
          << ", cups job id: " << job_id;

  return job_id;
}

// static
std::string PrintSystemCUPS::MakeFullPrinterName(
    const GURL& url, const std::string& short_printer_name) {
  std::string full_name;
  full_name += "\\\\";
  full_name += url.host();
  if (!url.port().empty()) {
    full_name += ":";
    full_name += url.port();
  }
  full_name += "\\";
  full_name += short_printer_name;
  return full_name;
}

PrintServerInfoCUPS* PrintSystemCUPS::FindServerByFullName(
    const std::string& full_printer_name, std::string* short_printer_name) {
  size_t front = full_printer_name.find("\\\\");
  size_t separator = full_printer_name.find("\\", 2);
  if (front == std::string::npos || separator == std::string::npos) {
    LOG(WARNING) << "CP_CUPS: Invalid UNC"
                 << ", printer name: " << full_printer_name;
    return nullptr;
  }
  std::string server = full_printer_name.substr(2, separator - 2);

  for (auto& print_server : print_servers_) {
    std::string cur_server;
    cur_server += print_server.url.host();
    if (!print_server.url.port().empty()) {
      cur_server += ":";
      cur_server += print_server.url.port();
    }
    if (cur_server == server) {
      *short_printer_name = full_printer_name.substr(separator + 1);
      return &print_server;
    }
  }

  LOG(WARNING) << "CP_CUPS: Server not found"
               << ", printer name: " << full_printer_name;
  return nullptr;
}

void PrintSystemCUPS::RunCapsCallback(
    PrinterCapsAndDefaultsCallback callback,
    bool succeeded,
    const std::string& printer_name,
    const printing::PrinterCapsAndDefaults& printer_info) {
  std::move(callback).Run(succeeded, printer_name, printer_info);
}

}  // namespace cloud_print
