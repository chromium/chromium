// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/print_system.h"

#include <windows.h>
#include <winspool.h>
#include <wrl/client.h>

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/free_deleter.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_hdc.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/service/cloud_print/cdd_conversion_win.h"
#include "chrome/service/service_process.h"
#include "chrome/service/service_utility_process_host.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/printing/common/cloud_print_cdd_conversion.h"
#include "printing/backend/win_helper.h"
#include "printing/emf_win.h"
#include "printing/page_range.h"
#include "printing/pdf_render_settings.h"
#include "printing/printing_utils.h"
#include "ui/gfx/geometry/rect.h"

namespace cloud_print {

namespace {

bool CurrentlyOnServiceIOThread() {
  return g_service_process->io_task_runner()->BelongsToCurrentThread();
}

bool PostIOThreadTask(const base::Location& from_here, base::OnceClosure task) {
  return g_service_process->io_task_runner()->PostTask(from_here,
                                                       std::move(task));
}

class PrintSystemWatcherWin : public base::win::ObjectWatcher::Delegate {
 public:
  PrintSystemWatcherWin() {}
  ~PrintSystemWatcherWin() override { Stop(); }

  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnPrinterAdded() = 0;
    virtual void OnPrinterDeleted() = 0;
    virtual void OnPrinterChanged() = 0;
    virtual void OnJobChanged() = 0;
  };

  bool Start(const std::string& printer_name, Delegate* delegate) {
    scoped_refptr<printing::PrintBackend> print_backend(
        printing::PrintBackend::CreateInstance(nullptr));
    printer_info_ = print_backend->GetPrinterDriverInfo(printer_name);
    crash_keys::ScopedPrinterInfo crash_key(printer_info_);

    delegate_ = delegate;
    // An empty printer name means watch the current server, we need to pass
    // nullptr to OpenPrinterWithName().
    LPTSTR printer_name_to_use = nullptr;
    std::wstring printer_name_wide;
    if (!printer_name.empty()) {
      printer_name_wide = base::UTF8ToWide(printer_name);
      printer_name_to_use = const_cast<LPTSTR>(printer_name_wide.c_str());
    }
    bool ret = false;
    if (printer_.OpenPrinterWithName(printer_name_to_use)) {
      printer_change_.Set(FindFirstPrinterChangeNotification(
          printer_.Get(), PRINTER_CHANGE_PRINTER | PRINTER_CHANGE_JOB, 0,
          nullptr));
      if (printer_change_.IsValid()) {
        ret = watcher_.StartWatchingOnce(printer_change_.Get(), this);
      }
    }
    if (!ret) {
      Stop();
    }
    return ret;
  }

  bool Stop() {
    watcher_.StopWatching();
    printer_.Close();
    printer_change_.Close();
    return true;
  }

  // base::ObjectWatcher::Delegate method
  void OnObjectSignaled(HANDLE object) override {
    crash_keys::ScopedPrinterInfo crash_key(printer_info_);
    DWORD change = 0;
    FindNextPrinterChangeNotification(object, &change, nullptr, nullptr);

    if (change != ((PRINTER_CHANGE_PRINTER|PRINTER_CHANGE_JOB) &
                  (~PRINTER_CHANGE_FAILED_CONNECTION_PRINTER))) {
      // For printer connections, we get spurious change notifications with
      // all flags set except PRINTER_CHANGE_FAILED_CONNECTION_PRINTER.
      // Ignore these.
      if (change & PRINTER_CHANGE_ADD_PRINTER) {
        delegate_->OnPrinterAdded();
      } else if (change & PRINTER_CHANGE_DELETE_PRINTER) {
        delegate_->OnPrinterDeleted();
      } else if (change & PRINTER_CHANGE_SET_PRINTER) {
        delegate_->OnPrinterChanged();
      }
      if (change & PRINTER_CHANGE_JOB) {
        delegate_->OnJobChanged();
      }
    }
    watcher_.StartWatchingOnce(printer_change_.Get(), this);
  }

  bool GetCurrentPrinterInfo(printing::PrinterBasicInfo* printer_info) {
    DCHECK(printer_info);
    return InitBasicPrinterInfo(printer_.Get(), printer_info);
  }

 private:
  base::win::ObjectWatcher watcher_;
  printing::ScopedPrinterHandle printer_;  // The printer being watched
  // Returned by FindFirstPrinterChangeNotifier.
  printing::ScopedPrinterChangeHandle printer_change_;
  Delegate* delegate_ = nullptr;  // Delegate to notify
  std::string printer_info_;      // For crash reporting.
};

class PrintServerWatcherWin
  : public PrintSystem::PrintServerWatcher,
    public PrintSystemWatcherWin::Delegate {
 public:
  PrintServerWatcherWin() {}

  // PrintSystem::PrintServerWatcher implementation.
  bool StartWatching(
      PrintSystem::PrintServerWatcher::Delegate* delegate) override {
    delegate_ = delegate;
    return watcher_.Start(std::string(), this);
  }

  bool StopWatching() override {
    bool ret = watcher_.Stop();
    delegate_ = nullptr;
    return ret;
  }

  // PrintSystemWatcherWin::Delegate implementation.
  void OnPrinterAdded() override {
    delegate_->OnPrinterAdded();
  }
  void OnPrinterDeleted() override {}
  void OnPrinterChanged() override {}
  void OnJobChanged() override {}

 protected:
  ~PrintServerWatcherWin() override {}

 private:
  PrintSystem::PrintServerWatcher::Delegate* delegate_ = nullptr;
  PrintSystemWatcherWin watcher_;

  DISALLOW_COPY_AND_ASSIGN(PrintServerWatcherWin);
};

class PrinterWatcherWin
    : public PrintSystem::PrinterWatcher,
      public PrintSystemWatcherWin::Delegate {
 public:
  explicit PrinterWatcherWin(const std::string& printer_name)
      : printer_name_(printer_name) {}

  // PrintSystem::PrinterWatcher implementation.
  bool StartWatching(PrintSystem::PrinterWatcher::Delegate* delegate) override {
    delegate_ = delegate;
    return watcher_.Start(printer_name_, this);
  }

  bool StopWatching() override {
    bool ret = watcher_.Stop();
    delegate_ = nullptr;
    return ret;
  }

  bool GetCurrentPrinterInfo(
      printing::PrinterBasicInfo* printer_info) override {
    return watcher_.GetCurrentPrinterInfo(printer_info);
  }

  // PrintSystemWatcherWin::Delegate implementation.
  void OnPrinterAdded() override {
    NOTREACHED();
  }
  void OnPrinterDeleted() override {
    delegate_->OnPrinterDeleted();
  }
  void OnPrinterChanged() override {
    delegate_->OnPrinterChanged();
  }
  void OnJobChanged() override {
    delegate_->OnJobChanged();
  }

 protected:
  ~PrinterWatcherWin() override {}

 private:
  const std::string printer_name_;
  PrintSystem::PrinterWatcher::Delegate* delegate_ = nullptr;
  PrintSystemWatcherWin watcher_;

  DISALLOW_COPY_AND_ASSIGN(PrinterWatcherWin);
};

class JobSpoolerWin : public PrintSystem::JobSpooler {
 public:
  JobSpoolerWin() : core_(base::MakeRefCounted<Core>()) {}

  // PrintSystem::JobSpooler implementation.
  bool Spool(const std::string& print_ticket,
             const std::string& print_ticket_mime_type,
             const base::FilePath& print_data_file_path,
             const std::string& print_data_mime_type,
             const std::string& printer_name,
             const std::string& job_title,
             const std::vector<std::string>& tags,
             JobSpooler::Delegate* delegate) override {
    // TODO(gene): add tags handling.
    scoped_refptr<printing::PrintBackend> print_backend(
        printing::PrintBackend::CreateInstance(nullptr));
    crash_keys::ScopedPrinterInfo crash_key(
        print_backend->GetPrinterDriverInfo(printer_name));
    return core_->Spool(print_ticket, print_ticket_mime_type,
                        print_data_file_path, print_data_mime_type,
                        printer_name, job_title, delegate);
  }

 protected:
  ~JobSpoolerWin() override {}

 private:
  // We use a Core class because we want a separate RefCountedThreadSafe
  // implementation for ServiceUtilityProcessHost::Client.
  class Core : public ServiceUtilityProcessHost::Client,
               public base::win::ObjectWatcher::Delegate {
   public:
    Core() {}

    bool Spool(const std::string& print_ticket,
               const std::string& print_ticket_mime_type,
               const base::FilePath& print_data_file_path,
               const std::string& print_data_mime_type,
               const std::string& printer_name,
               const std::string& job_title,
               JobSpooler::Delegate* delegate) {
      if (delegate_) {
        // We are already in the process of printing.
        NOTREACHED();
        return false;
      }

      // We only support PDF and XPS documents for now.
      if (print_data_mime_type == kContentTypePDF) {
        base::string16 printer_wide = base::UTF8ToWide(printer_name);
        std::unique_ptr<DEVMODE, base::FreeDeleter> dev_mode;
        if (print_ticket_mime_type == kContentTypeJSON) {
          dev_mode = CjtToDevMode(printer_wide, print_ticket);
        } else {
          DCHECK_EQ(kContentTypeXML, print_ticket_mime_type);
          dev_mode = printing::XpsTicketToDevMode(printer_wide, print_ticket);
        }

        if (!dev_mode) {
          NOTREACHED();
          return false;
        }

        HDC dc = CreateDC(L"WINSPOOL", printer_wide.c_str(), nullptr,
                          dev_mode.get());
        if (!dc) {
          NOTREACHED();
          return false;
        }
        DOCINFO di = {0};
        di.cbSize = sizeof(DOCINFO);
        base::string16 doc_name = base::UTF8ToUTF16(job_title);
        DCHECK(printing::SimplifyDocumentTitle(doc_name) == doc_name);
        di.lpszDocName = doc_name.c_str();
        job_id_ = StartDoc(dc, &di);
        if (job_id_ <= 0)
          return false;

        printer_dc_.Set(dc);
        saved_dc_ = SaveDC(printer_dc_.Get());
        delegate_ = delegate;
        RenderPDFPages(print_data_file_path,
                       printing::IsDevModeWithColor(dev_mode.get()));
        return true;
      }

      if (print_data_mime_type == kContentTypeXPS) {
        DCHECK(print_ticket_mime_type == kContentTypeXML);
        bool ret = PrintXPSDocument(printer_name,
                                    job_title,
                                    print_data_file_path,
                                    print_ticket);
        if (ret)
          delegate_ = delegate;
        return ret;
      }

      NOTREACHED();
      return false;
    }

    void PreparePageDCForPrinting(HDC, float scale_factor) {
      SetGraphicsMode(printer_dc_.Get(), GM_ADVANCED);
      // Setup the matrix to translate and scale to the right place. Take in
      // account the scale factor.
      // Note that the printing output is relative to printable area of
      // the page. That is 0,0 is offset by PHYSICALOFFSETX/Y from the page.
      int offset_x = ::GetDeviceCaps(printer_dc_.Get(), PHYSICALOFFSETX);
      int offset_y = ::GetDeviceCaps(printer_dc_.Get(), PHYSICALOFFSETY);
      XFORM xform = {0};
      xform.eDx = static_cast<float>(-offset_x);
      xform.eDy = static_cast<float>(-offset_y);
      xform.eM11 = xform.eM22 = 1.0f / scale_factor;
      SetWorldTransform(printer_dc_.Get(), &xform);
    }

    // ServiceUtilityProcessHost::Client implementation.
    void OnRenderPDFPagesToMetafilePageDone(
        float scale_factor,
        const printing::MetafilePlayer& emf) override {
      PreparePageDCForPrinting(printer_dc_.Get(), scale_factor);
      ::StartPage(printer_dc_.Get());
      emf.SafePlayback(printer_dc_.Get());
      ::EndPage(printer_dc_.Get());
    }

    // ServiceUtilityProcessHost::Client implementation.
    void OnRenderPDFPagesToMetafileDone(bool success) override {
      PrintJobDone(success);
    }

    void OnChildDied() override { PrintJobDone(false); }

    // base::win::ObjectWatcher::Delegate implementation.
    void OnObjectSignaled(HANDLE object) override {
      DCHECK(xps_print_job_.Get());
      DCHECK(object == job_progress_event_.Get());
      ResetEvent(job_progress_event_.Get());
      if (!delegate_)
        return;
      XPS_JOB_STATUS job_status = {0};
      xps_print_job_->GetJobStatus(&job_status);
      if ((job_status.completion == XPS_JOB_CANCELLED) ||
          (job_status.completion == XPS_JOB_FAILED)) {
        delegate_->OnJobSpoolFailed();
      } else if (job_status.jobId ||
                  (job_status.completion == XPS_JOB_COMPLETED)) {
        // Note: In the case of the XPS document being printed to the
        // Microsoft XPS Document Writer, it seems to skip spooling the job
        // and goes to the completed state without ever assigning a job id.
        delegate_->OnJobSpoolSucceeded(job_status.jobId);
      } else {
        job_progress_watcher_.StopWatching();
        job_progress_watcher_.StartWatchingOnce(
            job_progress_event_.Get(), this);
      }
    }

   private:
    ~Core() override {}

    // Helper class to allow PrintXPSDocument() to have multiple exits.
    class PrintJobCanceler {
     public:
      explicit PrintJobCanceler(Microsoft::WRL::ComPtr<IXpsPrintJob>* job_ptr)
          : job_ptr_(job_ptr) {}
      ~PrintJobCanceler() {
        if (job_ptr_ && job_ptr_->Get()) {
          (*job_ptr_)->Cancel();
          job_ptr_->Reset();
        }
      }

      void reset() { job_ptr_ = nullptr; }

     private:
      Microsoft::WRL::ComPtr<IXpsPrintJob>* job_ptr_;

      DISALLOW_COPY_AND_ASSIGN(PrintJobCanceler);
    };

    void PrintJobDone(bool success) {
      // If there is no delegate, then there is nothing pending to process.
      if (!delegate_)
        return;
      RestoreDC(printer_dc_.Get(), saved_dc_);
      EndDoc(printer_dc_.Get());
      if (success) {
        delegate_->OnJobSpoolSucceeded(job_id_);
      } else {
        delegate_->OnJobSpoolFailed();
      }
      delegate_ = nullptr;
    }

    void RenderPDFPages(const base::FilePath& pdf_path, bool use_color) {
      gfx::Size printer_dpi =
          gfx::Size(::GetDeviceCaps(printer_dc_.Get(), LOGPIXELSX),
                    ::GetDeviceCaps(printer_dc_.Get(), LOGPIXELSY));
      int dc_width = GetDeviceCaps(printer_dc_.Get(), PHYSICALWIDTH);
      int dc_height = GetDeviceCaps(printer_dc_.Get(), PHYSICALHEIGHT);
      gfx::Rect render_area(0, 0, dc_width, dc_height);
      PostIOThreadTask(
          FROM_HERE,
          base::BindOnce(&JobSpoolerWin::Core::RenderPDFPagesInSandbox, this,
                         pdf_path, render_area, printer_dpi, use_color,
                         base::ThreadTaskRunnerHandle::Get()));
    }

    void RenderPDFPagesInSandbox(
        const base::FilePath& pdf_path,
        const gfx::Rect& render_area,
        const gfx::Size& render_dpi,
        bool use_color,
        scoped_refptr<base::SingleThreadTaskRunner> client_task_runner) {
      DCHECK(CurrentlyOnServiceIOThread());
      auto utility_host = std::make_unique<ServiceUtilityProcessHost>(
          this, client_task_runner.get());
      // TODO(gene): For now we disabling autorotation for CloudPrinting.
      // Landscape/Portrait setting is passed in the print ticket and
      // server is generating portrait PDF always.
      // We should enable autorotation once server will be able to generate
      // PDF that matches paper size and orientation.
      if (utility_host->StartRenderPDFPagesToMetafile(
              pdf_path, printing::PdfRenderSettings(
                            render_area, gfx::Point(0, 0), render_dpi,
                            /*autorotate=*/false, use_color,
                            printing::PdfRenderSettings::Mode::NORMAL))) {
        // The object will self-destruct when the child process dies.
        ignore_result(utility_host.release());
      } else {
        client_task_runner->PostTask(
            FROM_HERE, base::BindOnce(&Core::PrintJobDone, this, false));
      }
    }

    bool PrintXPSDocument(const std::string& printer_name,
                          const std::string& job_title,
                          const base::FilePath& xps_path,
                          const std::string& print_ticket) {
      base::File xps_file(xps_path, base::File::FLAG_OPEN |
                                        base::File::FLAG_READ |
                                        base::File::FLAG_DELETE_ON_CLOSE);
      if (!printing::XPSPrintModule::Init())
        return false;

      job_progress_event_.Set(CreateEvent(nullptr, TRUE, FALSE, nullptr));
      if (!job_progress_event_.Get())
        return false;

      PrintJobCanceler job_canceler(&xps_print_job_);
      Microsoft::WRL::ComPtr<IXpsPrintJobStream> doc_stream;
      Microsoft::WRL::ComPtr<IXpsPrintJobStream> print_ticket_stream;
      if (FAILED(printing::XPSPrintModule::StartXpsPrintJob(
              base::UTF8ToWide(printer_name).c_str(),
              base::UTF8ToWide(job_title).c_str(), nullptr,
              job_progress_event_.Get(), nullptr, nullptr, 0,
              xps_print_job_.GetAddressOf(), doc_stream.GetAddressOf(),
              print_ticket_stream.GetAddressOf()))) {
        return false;
      }

      ULONG print_bytes_written = 0;
      if (FAILED(print_ticket_stream->Write(print_ticket.c_str(),
                                            print_ticket.length(),
                                            &print_bytes_written))) {
        return false;
      }
      DCHECK_EQ(print_ticket.length(), print_bytes_written);
      if (FAILED(print_ticket_stream->Close()))
        return false;

      int64_t file_size = xps_file.GetLength();
      if (file_size <= 0)
        return false;

      std::unique_ptr<char[]> document_data(new char[file_size]);
      int bytes_read = xps_file.Read(0, document_data.get(), file_size);
      if (bytes_read != file_size)
        return false;

      ULONG doc_bytes_written = 0;
      if (FAILED(doc_stream->Write(document_data.get(), file_size,
                                   &doc_bytes_written))) {
        return false;
      }
      DCHECK_EQ(file_size, doc_bytes_written);
      if (FAILED(doc_stream->Close()))
        return false;

      job_progress_watcher_.StartWatchingOnce(job_progress_event_.Get(), this);
      job_canceler.reset();
      return true;
    }

    PlatformJobId job_id_ = -1;
    PrintSystem::JobSpooler::Delegate* delegate_ = nullptr;
    int saved_dc_ = 0;
    base::win::ScopedCreateDC printer_dc_;
    base::win::ScopedHandle job_progress_event_;
    base::win::ObjectWatcher job_progress_watcher_;
    Microsoft::WRL::ComPtr<IXpsPrintJob> xps_print_job_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };
  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(JobSpoolerWin);
};

// A helper class to handle the response from the utility process to the
// request to fetch printer capabilities and defaults.
class PrinterCapsHandler : public ServiceUtilityProcessHost::Client {
 public:
  PrinterCapsHandler(const std::string& printer_name,
                     PrintSystem::PrinterCapsAndDefaultsCallback callback)
      : printer_name_(printer_name), callback_(std::move(callback)) {}

  // ServiceUtilityProcessHost::Client implementation.
  void OnChildDied() override {
    OnGetPrinterCapsAndDefaults(false, printer_name_,
                                printing::PrinterCapsAndDefaults());
  }

  void OnGetPrinterCapsAndDefaults(
      bool succeeded,
      const std::string& printer_name,
      const printing::PrinterCapsAndDefaults& caps_and_defaults) override {
    std::move(callback_).Run(succeeded, printer_name, caps_and_defaults);
    Release();
  }

  void OnGetPrinterSemanticCapsAndDefaults(
      bool succeeded,
      const std::string& printer_name,
      const printing::PrinterSemanticCapsAndDefaults& semantic_info) override {
    printing::PrinterCapsAndDefaults printer_info;
    if (succeeded) {
      printer_info.caps_mime_type = kContentTypeJSON;
      base::JSONWriter::WriteWithOptions(
          PrinterSemanticCapsAndDefaultsToCdd(semantic_info),
          base::JSONWriter::OPTIONS_PRETTY_PRINT,
          &printer_info.printer_capabilities);
    }
    std::move(callback_).Run(succeeded, printer_name, printer_info);
    Release();
  }

  void StartGetPrinterCapsAndDefaults() {
    PostIOThreadTask(
        FROM_HERE,
        base::BindOnce(&PrinterCapsHandler::GetPrinterCapsAndDefaultsImpl, this,
                       base::ThreadTaskRunnerHandle::Get()));
  }

  void StartGetPrinterSemanticCapsAndDefaults() {
    PostIOThreadTask(
        FROM_HERE,
        base::BindOnce(
            &PrinterCapsHandler::GetPrinterSemanticCapsAndDefaultsImpl, this,
            base::ThreadTaskRunnerHandle::Get()));
  }

 private:
  ~PrinterCapsHandler() override {}

  void GetPrinterCapsAndDefaultsImpl(
      scoped_refptr<base::SingleThreadTaskRunner> client_task_runner) {
    DCHECK(CurrentlyOnServiceIOThread());
    auto utility_host = std::make_unique<ServiceUtilityProcessHost>(
        this, client_task_runner.get());
    if (utility_host->StartGetPrinterCapsAndDefaults(printer_name_)) {
      // The object will self-destruct when the child process dies.
      ignore_result(utility_host.release());
    } else {
      client_task_runner->PostTask(
          FROM_HERE, base::BindOnce(&PrinterCapsHandler::OnChildDied, this));
    }
  }

  void GetPrinterSemanticCapsAndDefaultsImpl(
      scoped_refptr<base::SingleThreadTaskRunner> client_task_runner) {
    DCHECK(CurrentlyOnServiceIOThread());
    auto utility_host = std::make_unique<ServiceUtilityProcessHost>(
        this, client_task_runner.get());
    if (utility_host->StartGetPrinterSemanticCapsAndDefaults(printer_name_)) {
      // The object will self-destruct when the child process dies.
      ignore_result(utility_host.release());
    } else {
      client_task_runner->PostTask(
          FROM_HERE, base::BindOnce(&PrinterCapsHandler::OnChildDied, this));
    }
  }

  const std::string printer_name_;
  PrintSystem::PrinterCapsAndDefaultsCallback callback_;
};

class PrintSystemWin : public PrintSystem {
 public:
  PrintSystemWin();

  // PrintSystem implementation.
  PrintSystemResult Init() override;
  PrintSystem::PrintSystemResult EnumeratePrinters(
      printing::PrinterList* printer_list) override;
  void GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      PrinterCapsAndDefaultsCallback callback) override;
  bool IsValidPrinter(const std::string& printer_name) override;
  bool ValidatePrintTicket(
      const std::string& printer_name,
      const std::string& print_ticket_data,
      const std::string& print_ticket_data_mime_type) override;
  bool GetJobDetails(const std::string& printer_name,
                     PlatformJobId job_id,
                     PrintJobDetails* job_details) override;
  PrintSystem::PrintServerWatcher* CreatePrintServerWatcher() override;
  PrintSystem::PrinterWatcher* CreatePrinterWatcher(
      const std::string& printer_name) override;
  PrintSystem::JobSpooler* CreateJobSpooler() override;
  bool UseCddAndCjt() override;
  std::string GetSupportedMimeTypes() override;

 private:
  ~PrintSystemWin() override {}

  std::string GetPrinterDriverInfo(const std::string& printer_name) const;

  scoped_refptr<printing::PrintBackend> print_backend_;
  bool use_cdd_ = true;
  DISALLOW_COPY_AND_ASSIGN(PrintSystemWin);
};

PrintSystemWin::PrintSystemWin()
    : print_backend_(printing::PrintBackend::CreateInstance(nullptr)) {}

PrintSystem::PrintSystemResult PrintSystemWin::Init() {
  use_cdd_ = !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableCloudPrintXps);

  if (!use_cdd_)
    use_cdd_ = !printing::XPSModule::Init();

  if (!use_cdd_) {
    HPTPROVIDER provider = nullptr;
    HRESULT hr = printing::XPSModule::OpenProvider(L"", 1, &provider);
    if (provider)
      printing::XPSModule::CloseProvider(provider);
    // Use cdd if error is different from expected.
    use_cdd_ = (hr != HRESULT_FROM_WIN32(ERROR_INVALID_PRINTER_NAME));
  }

  return PrintSystemResult(true, std::string());
}

PrintSystem::PrintSystemResult PrintSystemWin::EnumeratePrinters(
    printing::PrinterList* printer_list) {
  bool ret = print_backend_->EnumeratePrinters(printer_list);
  return PrintSystemResult(ret, std::string());
}

void PrintSystemWin::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    PrinterCapsAndDefaultsCallback callback) {
  // Launch as child process to retrieve the capabilities and defaults because
  // this involves invoking a printer driver DLL and crashes have been known to
  // occur.
  PrinterCapsHandler* handler =
      new PrinterCapsHandler(printer_name, std::move(callback));
  handler->AddRef();
  if (use_cdd_)
    handler->StartGetPrinterSemanticCapsAndDefaults();
  else
    handler->StartGetPrinterCapsAndDefaults();
}

bool PrintSystemWin::IsValidPrinter(const std::string& printer_name) {
  return print_backend_->IsValidPrinter(printer_name);
}

bool PrintSystemWin::ValidatePrintTicket(
    const std::string& printer_name,
    const std::string& print_ticket_data,
    const std::string& print_ticket_data_mime_type) {
  crash_keys::ScopedPrinterInfo crash_key(GetPrinterDriverInfo(printer_name));

  if (use_cdd_) {
    return print_ticket_data_mime_type == kContentTypeJSON &&
           IsValidCjt(print_ticket_data);
  }
  DCHECK(print_ticket_data_mime_type == kContentTypeXML);

  printing::ScopedXPSInitializer xps_initializer;
  CHECK(xps_initializer.initialized());

  HPTPROVIDER provider = nullptr;
  printing::XPSModule::OpenProvider(base::UTF8ToWide(printer_name), 1,
                                    &provider);
  if (!provider)
    return false;

  bool ret;
  {
    Microsoft::WRL::ComPtr<IStream> print_ticket_stream;
    CreateStreamOnHGlobal(nullptr, TRUE, print_ticket_stream.GetAddressOf());
    ULONG bytes_written = 0;
    print_ticket_stream->Write(print_ticket_data.c_str(),
                               print_ticket_data.length(),
                               &bytes_written);
    DCHECK(bytes_written == print_ticket_data.length());
    LARGE_INTEGER pos = {};
    ULARGE_INTEGER new_pos = {};
    print_ticket_stream->Seek(pos, STREAM_SEEK_SET, &new_pos);
    base::win::ScopedBstr error;
    Microsoft::WRL::ComPtr<IStream> result_ticket_stream;
    CreateStreamOnHGlobal(nullptr, TRUE, result_ticket_stream.GetAddressOf());
    ret = SUCCEEDED(printing::XPSModule::MergeAndValidatePrintTicket(
        provider, print_ticket_stream.Get(), nullptr, kPTJobScope,
        result_ticket_stream.Get(), error.Receive()));
    printing::XPSModule::CloseProvider(provider);
  }
  return ret;
}

bool PrintSystemWin::GetJobDetails(const std::string& printer_name,
                                   PlatformJobId job_id,
                                   PrintJobDetails* job_details) {
  crash_keys::ScopedPrinterInfo crash_key(
      print_backend_->GetPrinterDriverInfo(printer_name));
  DCHECK(job_details);
  printing::ScopedPrinterHandle printer_handle;
  std::wstring printer_name_wide = base::UTF8ToWide(printer_name);
  printer_handle.OpenPrinterWithName(printer_name_wide.c_str());
  DCHECK(printer_handle.IsValid());
  bool ret = false;
  if (printer_handle.IsValid()) {
    DWORD bytes_needed = 0;
    GetJob(printer_handle.Get(), job_id, 1, nullptr, 0, &bytes_needed);
    DWORD last_error = GetLastError();
    if (ERROR_INVALID_PARAMETER != last_error) {
      // ERROR_INVALID_PARAMETER normally means that the job id is not valid.
      DCHECK(last_error == ERROR_INSUFFICIENT_BUFFER);
      std::unique_ptr<BYTE[]> job_info_buffer(new BYTE[bytes_needed]);
      if (GetJob(printer_handle.Get(), job_id, 1, job_info_buffer.get(),
                 bytes_needed, &bytes_needed)) {
        JOB_INFO_1* job_info =
            reinterpret_cast<JOB_INFO_1*>(job_info_buffer.get());
        if (job_info->pStatus) {
          base::WideToUTF8(job_info->pStatus, wcslen(job_info->pStatus),
                           &job_details->status_message);
        }
        job_details->platform_status_flags = job_info->Status;
        if ((job_info->Status & JOB_STATUS_COMPLETE) ||
            (job_info->Status & JOB_STATUS_PRINTED)) {
          job_details->status = PRINT_JOB_STATUS_COMPLETED;
        } else if (job_info->Status & JOB_STATUS_ERROR) {
          job_details->status = PRINT_JOB_STATUS_ERROR;
        } else {
          job_details->status = PRINT_JOB_STATUS_IN_PROGRESS;
        }
        job_details->total_pages = job_info->TotalPages;
        job_details->pages_printed = job_info->PagesPrinted;
        ret = true;
      }
    }
  }
  return ret;
}

PrintSystem::PrintServerWatcher*
PrintSystemWin::CreatePrintServerWatcher() {
  return new PrintServerWatcherWin();
}

PrintSystem::PrinterWatcher* PrintSystemWin::CreatePrinterWatcher(
    const std::string& printer_name) {
  DCHECK(!printer_name.empty());
  return new PrinterWatcherWin(printer_name);
}

PrintSystem::JobSpooler* PrintSystemWin::CreateJobSpooler() {
  return new JobSpoolerWin();
}

bool PrintSystemWin::UseCddAndCjt() {
  return use_cdd_;
}

std::string PrintSystemWin::GetSupportedMimeTypes() {
  std::string result;
  if (!use_cdd_) {
    result = kContentTypeXPS;
    result += ",";
  }
  result += kContentTypePDF;
  return result;
}

std::string PrintSystemWin::GetPrinterDriverInfo(
    const std::string& printer_name) const {
  return print_backend_->GetPrinterDriverInfo(printer_name);
}

}  // namespace

scoped_refptr<PrintSystem> PrintSystem::CreateInstance(
    const base::DictionaryValue* print_system_settings) {
  return base::MakeRefCounted<PrintSystemWin>();
}

}  // namespace cloud_print
