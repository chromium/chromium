// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_PRINT_SYSTEM_H_
#define CHROME_SERVICE_CLOUD_PRINT_PRINT_SYSTEM_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "printing/backend/print_backend.h"

namespace base {
class DictionaryValue;
class FilePath;
}

namespace printing {
struct PrinterBasicInfo;
struct PrinterCapsAndDefaults;
}

// This is the interface for platform-specific code for cloud print
namespace cloud_print {

typedef int PlatformJobId;

enum PrintJobStatus {
  PRINT_JOB_STATUS_INVALID,
  PRINT_JOB_STATUS_IN_PROGRESS,
  PRINT_JOB_STATUS_ERROR,
  PRINT_JOB_STATUS_COMPLETED,
  PRINT_JOB_STATUS_MAX,
};

struct PrintJobDetails {
  PrintJobDetails();

  void Clear();

  bool operator ==(const PrintJobDetails& other) const {
    return (status == other.status) &&
           (platform_status_flags == other.platform_status_flags) &&
           (status_message == other.status_message) &&
           (total_pages == other.total_pages) &&
           (pages_printed == other.pages_printed);
  }

  bool operator !=(const PrintJobDetails& other) const {
    return !(*this == other);
  }

  PrintJobStatus status;
  int platform_status_flags;
  std::string status_message;
  int total_pages;
  int pages_printed;
};

// PrintSystem class will provide interface for different printing systems
// (Windows, CUPS) to implement. User will call CreateInstance() to
// obtain available printing system.
// Please note, that PrintSystem is not platform specific, but rather
// print system specific. For example, CUPS is available on both Linux and Mac,
// but not available on ChromeOS, etc. This design allows us to add more
// functionality on some platforms, while reusing core (CUPS) functions.
class PrintSystem : public base::RefCountedThreadSafe<PrintSystem> {
 public:
  class PrintServerWatcher
      : public base::RefCountedThreadSafe<PrintServerWatcher> {
   public:
    // Callback interface for new printer notifications.
    class Delegate {
     public:
      virtual void OnPrinterAdded() = 0;
      // TODO(gene): Do we need OnPrinterDeleted notification here?

     protected:
      virtual ~Delegate() {}
    };

    virtual bool StartWatching(PrintServerWatcher::Delegate* delegate) = 0;
    virtual bool StopWatching() = 0;

   protected:
    friend class base::RefCountedThreadSafe<PrintServerWatcher>;
    virtual ~PrintServerWatcher();
  };

  class PrinterWatcher : public base::RefCountedThreadSafe<PrinterWatcher> {
   public:
    // Callback interface for printer updates notifications.
    class Delegate {
     public:
      virtual void OnPrinterDeleted() = 0;
      virtual void OnPrinterChanged() = 0;
      virtual void OnJobChanged() = 0;

     protected:
      virtual ~Delegate() {}
    };

    virtual bool StartWatching(PrinterWatcher::Delegate* delegate) = 0;
    virtual bool StopWatching() = 0;
    virtual bool GetCurrentPrinterInfo(
        printing::PrinterBasicInfo* printer_info) = 0;

   protected:
    friend class base::RefCountedThreadSafe<PrinterWatcher>;
    virtual ~PrinterWatcher();
  };

  class JobSpooler : public base::RefCountedThreadSafe<JobSpooler> {
   public:
    // Callback interface for JobSpooler notifications.
    class Delegate {
     public:
      virtual void OnJobSpoolSucceeded(const PlatformJobId& job_id) = 0;
      virtual void OnJobSpoolFailed() = 0;

     protected:
      virtual ~Delegate() {}
    };

    // Spool job to the printer asynchronously. Caller will be notified via
    // |delegate|. Note that only one print job can be in progress at any given
    // time. Subsequent calls to Spool (before the Delegate::OnJobSpoolSucceeded
    // or Delegate::OnJobSpoolFailed methods are called) can fail.
    virtual bool Spool(const std::string& print_ticket,
                       const std::string& print_ticket_mime_type,
                       const base::FilePath& print_data_file_path,
                       const std::string& print_data_mime_type,
                       const std::string& printer_name,
                       const std::string& job_title,
                       const std::vector<std::string>& tags,
                       JobSpooler::Delegate* delegate) = 0;
   protected:
    friend class base::RefCountedThreadSafe<JobSpooler>;
    virtual ~JobSpooler();
  };

  class PrintSystemResult {
   public:
    PrintSystemResult(bool succeeded, const std::string& message)
        : succeeded_(succeeded), message_(message) { }
    bool succeeded() const { return succeeded_; }
    std::string message() const { return message_; }

   private:
    PrintSystemResult() {}

    bool succeeded_;
    std::string message_;
  };

  using PrinterCapsAndDefaultsCallback = base::OnceCallback<
      void(bool, const std::string&, const printing::PrinterCapsAndDefaults&)>;

  // Initialize print system. This need to be called before any other function
  // of PrintSystem.
  virtual PrintSystemResult Init() = 0;

  // Enumerates the list of installed local and network printers.
  virtual PrintSystemResult EnumeratePrinters(
      printing::PrinterList* printer_list) = 0;

  // Gets the capabilities and defaults for a specific printer asynchronously.
  virtual void GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      PrinterCapsAndDefaultsCallback callback) = 0;

  // Returns true if printer_name points to a valid printer.
  virtual bool IsValidPrinter(const std::string& printer_name) = 0;

  // Returns true if ticket is valid.
  virtual bool ValidatePrintTicket(
      const std::string& printer_name,
      const std::string& print_ticket_data,
      const std::string& print_ticket_mime_type) = 0;

  // Get details for already spooled job.
  virtual bool GetJobDetails(const std::string& printer_name,
                             PlatformJobId job_id,
                             PrintJobDetails* job_details) = 0;

  // Factory methods to create corresponding watcher. Callee is responsible
  // for deleting objects. Return NULL if failed.
  virtual PrintServerWatcher* CreatePrintServerWatcher() = 0;
  virtual PrinterWatcher* CreatePrinterWatcher(
      const std::string& printer_name) = 0;
  virtual JobSpooler* CreateJobSpooler() = 0;

  // Returns a true if connector should use CDD for capabilities and CJT as
  // print ticket.
  virtual bool UseCddAndCjt() = 0;

  // Returns a comma separated list of mimetypes for print data that are
  // supported by this print system. The format of this string is the same as
  // that used for the HTTP Accept: header.
  virtual std::string GetSupportedMimeTypes() = 0;

  // Generate unique for proxy.
  static std::string GenerateProxyId();

  // Call this function to obtain printing system for specified print server.
  // If print settings are NULL, default settings will be used.
  // Return NULL if no print system available.
  static scoped_refptr<PrintSystem> CreateInstance(
      const base::DictionaryValue* print_system_settings);

 protected:
  friend class base::RefCountedThreadSafe<PrintSystem>;
  virtual ~PrintSystem();
};

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_PRINT_SYSTEM_H_
