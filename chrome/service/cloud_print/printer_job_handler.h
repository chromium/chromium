// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_PRINTER_JOB_HANDLER_H_
#define CHROME_SERVICE_CLOUD_PRINT_PRINTER_JOB_HANDLER_H_

#include <list>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/service/cloud_print/cloud_print_url_fetcher.h"
#include "chrome/service/cloud_print/job_status_updater.h"
#include "chrome/service/cloud_print/printer_job_queue_handler.h"
#include "net/url_request/url_request_status.h"
#include "printing/backend/print_backend.h"
#include "url/gurl.h"

class URLFetcher;

// A class that handles cloud print jobs for a particular printer. This class
// imlements a state machine that transitions from Start to various states. The
// various states are shown in the below diagram.
// the status on the server.

//                            Start --> No pending tasks --> Done
//                              |
//                              |
//                              | Have Pending tasks
//                              |
//                              |
//                              | ---Update Pending----->
//                              |                       |
//                              |                       |
//                              |                       |
//                              |                 Update Printer info on server
//                              |                      Go to Stop
//                              |
//                              | Job Available
//                              |
//                              |
//                        Fetch Next Job Metadata
//                        Fetch Print Ticket
//                        Fetch Print Data
//                        Spool Print Job
//                        Create Job StatusUpdater for job
//                        Mark job as "in progress" on server
//     (On any unrecoverable error in any of the above steps go to Stop)
//                        Go to Stop
//                              |
//                              |
//                              |
//                              |
//                              |
//                              |
//                              |
//                             Stop
//               (If there are pending tasks go back to Start)

namespace cloud_print {

class PrinterJobHandler : public base::RefCountedThreadSafe<PrinterJobHandler>,
                          public CloudPrintURLFetcher::Delegate,
                          public JobStatusUpdater::Delegate,
                          public PrintSystem::PrinterWatcher::Delegate,
                          public PrintSystem::JobSpooler::Delegate {
 public:
  class Delegate {
   public:
     // Notify delegate about authentication error.
     virtual void OnAuthError() = 0;
     // Notify delegate that printer has been deleted.
     virtual void OnPrinterDeleted(const std::string& printer_name) = 0;

   protected:
     virtual ~Delegate() {}
  };

  struct PrinterInfoFromCloud {
    std::string printer_id;
    std::string caps_hash;
    std::string tags_hash;
    int current_xmpp_timeout = 0;
    int pending_xmpp_timeout = 0;

    PrinterInfoFromCloud();
    PrinterInfoFromCloud(const PrinterInfoFromCloud& other);
  };

  static void ReportsStats();

  PrinterJobHandler(const printing::PrinterBasicInfo& printer_info,
                    const PrinterInfoFromCloud& printer_info_from_server,
                    const GURL& cloud_print_server_url,
                    PrintSystem* print_system,
                    Delegate* delegate);

  bool Initialize();

  const std::string& GetPrinterName() const;

  // Requests a job check. |reason| is the reason for fetching the job. Used
  // for logging and diagnostc purposes.
  void CheckForJobs(const std::string& reason);

  // Shutdown everything (the process is exiting).
  void Shutdown();

  base::TimeTicks last_job_fetch_time() const { return last_job_fetch_time_; }

  // CloudPrintURLFetcher::Delegate implementation.
  CloudPrintURLFetcher::ResponseAction HandleRawResponse(
      const net::URLFetcher* source,
      const GURL& url,
      const net::URLRequestStatus& status,
      int response_code,
      const std::string& data) override;
  CloudPrintURLFetcher::ResponseAction HandleRawData(
      const net::URLFetcher* source,
      const GURL& url,
      const std::string& data) override;
  CloudPrintURLFetcher::ResponseAction HandleJSONData(
      const net::URLFetcher* source,
      const GURL& url,
      const base::Value& json_data,
      bool succeeded) override;
  void OnRequestGiveUp() override;
  CloudPrintURLFetcher::ResponseAction OnRequestAuthError() override;
  std::string GetAuthHeader() override;

  // JobStatusUpdater::Delegate implementation
  bool OnJobCompleted(JobStatusUpdater* updater) override;
  void OnAuthError() override;

  // PrinterWatcherDelegate implementation
  void OnPrinterDeleted() override;
  void OnPrinterChanged() override;
  void OnJobChanged() override;

  // JobSpoolerDelegate implementation.
  // Called on |print_thread_|.
  void OnJobSpoolSucceeded(const PlatformJobId& job_id) override;
  void OnJobSpoolFailed() override;

 private:
  friend class base::RefCountedThreadSafe<PrinterJobHandler>;

  enum PrintJobError {
    JOB_SUCCESS,
    JOB_DOWNLOAD_FAILED,
    JOB_VALIDATE_TICKET_FAILED,
    JOB_FAILED,
    JOB_MAX,
  };

  // Prototype for a JSON data handler.
  typedef CloudPrintURLFetcher::ResponseAction (
      PrinterJobHandler::*JSONDataHandler)(const net::URLFetcher* source,
                                           const GURL& url,
                                           const base::Value& json_data,
                                           bool succeeded);
  // Prototype for a data handler.
  typedef CloudPrintURLFetcher::ResponseAction (
      PrinterJobHandler::*DataHandler)(const net::URLFetcher* source,
                                       const GURL& url,
                                       const std::string& data);

  ~PrinterJobHandler() override;

  // Begin request handlers for each state in the state machine
  CloudPrintURLFetcher::ResponseAction HandlePrinterUpdateResponse(
      const net::URLFetcher* source,
      const GURL& url,
      const base::Value& json_data,
      bool succeeded);

  CloudPrintURLFetcher::ResponseAction HandleJobMetadataResponse(
      const net::URLFetcher* source,
      const GURL& url,
      const base::Value& json_data,
      bool succeeded);

  CloudPrintURLFetcher::ResponseAction HandlePrintTicketResponse(
      const net::URLFetcher* source,
      const GURL& url,
      const std::string& data);

  CloudPrintURLFetcher::ResponseAction HandlePrintDataResponse(
      const net::URLFetcher* source,
      const GURL& url,
      const std::string& data);

  CloudPrintURLFetcher::ResponseAction HandleInProgressStatusUpdateResponse(
      const net::URLFetcher* source,
      const GURL& url,
      const base::Value& json_data,
      bool succeeded);

  CloudPrintURLFetcher::ResponseAction HandleFailureStatusUpdateResponse(
      const net::URLFetcher* source,
      const GURL& url,
      const base::Value& json_data,
      bool succeeded);
  // End request handlers for each state in the state machine

  // Start the state machine. Based on the flags set this could mean updating
  // printer information, deleting the printer from the server or looking for
  // new print jobs
  void Start();

  // End the state machine. If there are pending tasks, we will post a Start
  // again.
  void Stop();

  void StartPrinting();
  void Reset();
  void UpdateJobStatus(PrintJobStatus status, PrintJobError error);

  // Run a job check as the result of a scheduled check
  void RunScheduledJobCheck();

  // Sets the next response handler to the specified JSON data handler.
  void SetNextJSONHandler(JSONDataHandler handler);
  // Sets the next response handler to the specified data handler.
  void SetNextDataHandler(DataHandler handler);

  void JobFailed(PrintJobError error);
  void JobSpooled(PlatformJobId local_job_id);
  // Returns false if printer info is up to date and no updating is needed.
  bool UpdatePrinterInfo();
  bool HavePendingTasks();
  void ValidatePrintTicketFailed();

  // Callback that asynchronously receives printer caps and defaults.
  void OnReceivePrinterCaps(
      bool succeeded,
      const std::string& printer_name,
      const printing::PrinterCapsAndDefaults& caps_and_defaults);

  // Called on |print_thread_|. It is not safe to access any members other than
  // |job_handler_task_runner_|, |job_spooler_| and |print_system_|.
  void DoPrint(const JobDetails& job_details,
               const std::string& printer_name);

  bool CurrentlyOnPrintThread() const;

  scoped_refptr<CloudPrintURLFetcher> request_;
  scoped_refptr<PrintSystem> print_system_;
  printing::PrinterBasicInfo printer_info_;
  PrinterInfoFromCloud printer_info_cloud_;
  const GURL cloud_print_server_url_;
  const std::string print_data_url_;
  JobDetails job_details_;
  Delegate* const delegate_;

  // Once the job has been spooled to the local spooler, this specifies the
  // job id of the job on the local spooler.
  PlatformJobId local_job_id_ = -1;

  // The next response handler can either be a JSONDataHandler or a
  // DataHandler (depending on the current request being made).
  JSONDataHandler next_json_data_handler_ = nullptr;
  DataHandler next_data_handler_ = nullptr;
  // The thread on which the actual print operation happens
  base::Thread print_thread_;
  // The Job spooler object. This is only non-NULL during a print operation.
  // It lives and dies on |print_thread_|
  scoped_refptr<PrintSystem::JobSpooler> job_spooler_;
  // The task runner representing the thread on which this object was created.
  // Used by the print thread.
  scoped_refptr<base::SingleThreadTaskRunner> job_handler_task_runner_;

  // There may be pending tasks in the message queue when Shutdown is called.
  // We set this flag so as to do nothing in those tasks.
  bool shutting_down_ = false;

  // A string indicating the reason we are fetching jobs from the server
  // (used to specify the reason in the fetch URL).
  std::string job_fetch_reason_;
  // Flags that specify various pending server updates
  bool job_check_pending_ = false;
  bool printer_update_pending_ = true;

  // Some task in the state machine is in progress.
  bool task_in_progress_ = false;
  scoped_refptr<PrintSystem::PrinterWatcher> printer_watcher_;

  using JobStatusUpdaterList = std::list<scoped_refptr<JobStatusUpdater>>;
  JobStatusUpdaterList job_status_updater_list_;

  // Manages parsing the job queue
  PrinterJobQueueHandler job_queue_handler_;

  base::TimeTicks last_job_fetch_time_;

  base::Time job_start_time_;
  base::Time spooling_start_time_;

  base::WeakPtrFactory<PrinterJobHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrinterJobHandler);
};

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_PRINTER_JOB_HANDLER_H_
