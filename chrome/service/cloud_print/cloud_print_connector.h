// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_CONNECTOR_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_CONNECTOR_H_

#include <list>
#include <map>
#include <string>

#include "base/macros.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/service/cloud_print/connector_settings.h"
#include "chrome/service/cloud_print/print_system.h"
#include "chrome/service/cloud_print/printer_job_handler.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace cloud_print {

// CloudPrintConnector handles top printer management tasks.
//  - Matching local and cloud printers
//  - Registration of local printers
//  - Deleting cloud printers
// All tasks are posted to the common queue (PendingTasks) and executed
// one-by-one in FIFO order.
// CloudPrintConnector will notify client over Client interface.
class CloudPrintConnector
    : public base::RefCountedThreadSafe<CloudPrintConnector>,
      private PrintSystem::PrintServerWatcher::Delegate,
      private PrinterJobHandler::Delegate,
      private CloudPrintURLFetcher::Delegate {
 public:
  class Client {
   public:
    virtual void OnAuthFailed() = 0;
    virtual void OnXmppPingUpdated(int ping_timeout) = 0;
   protected:
     virtual ~Client() {}
  };

  CloudPrintConnector(Client* client,
                      const ConnectorSettings& settings,
                      const net::PartialNetworkTrafficAnnotationTag&
                          partial_traffic_annotation);

  bool Start();
  void Stop();
  bool IsRunning();

  // Return list of printer ids registered with CloudPrint.
  std::list<std::string> GetPrinterIds() const;

  // Check for jobs for specific printer. If printer id is empty
  // jobs will be checked for all available printers.
  void CheckForJobs(const std::string& reason, const std::string& printer_id);

  // Update settings for specific printer.
  void UpdatePrinterSettings(const std::string& printer_id);

 private:
  friend class base::RefCountedThreadSafe<CloudPrintConnector>;

  // Prototype for a response handler.
  typedef CloudPrintURLFetcher::ResponseAction (
      CloudPrintConnector::*ResponseHandler)(const net::URLFetcher* source,
                                             const GURL& url,
                                             const base::Value& json_data,
                                             bool succeeded);

  enum PendingTaskType {
    PENDING_PRINTERS_NONE,
    PENDING_PRINTERS_AVAILABLE,
    PENDING_PRINTER_REGISTER,
    PENDING_PRINTER_DELETE
  };

  // TODO(vitalybuka): Consider delete pending_tasks_ and just use MessageLoop.
  struct PendingTask {
    PendingTaskType type;
    // Optional members, depending on type.
    std::string printer_id;  // For pending delete.
    printing::PrinterBasicInfo printer_info;  // For pending registration.

    PendingTask() : type(PENDING_PRINTERS_NONE) {}
    ~PendingTask() {}
  };

  ~CloudPrintConnector() override;

  // PrintServerWatcherDelegate implementation:
  void OnPrinterAdded() override;

  // PrinterJobHandler::Delegate implementation:
  void OnPrinterDeleted(const std::string& printer_name) override;
  void OnAuthError() override;

  // CloudPrintURLFetcher::Delegate implementation:
  CloudPrintURLFetcher::ResponseAction HandleRawData(
      const net::URLFetcher* source,
      const GURL& url,
      const std::string& data) override;
  CloudPrintURLFetcher::ResponseAction HandleJSONData(
      const net::URLFetcher* source,
      const GURL& url,
      const base::Value& json_data,
      bool succeeded) override;
  CloudPrintURLFetcher::ResponseAction OnRequestAuthError() override;
  std::string GetAuthHeader() override;

  // Begin response handlers
  CloudPrintURLFetcher::ResponseAction HandlePrinterListResponse(
      const net::URLFetcher* source,
      const GURL& url,
      const base::Value& json_data,
      bool succeeded);

  CloudPrintURLFetcher::ResponseAction HandlePrinterListResponseSettingsUpdate(
      const net::URLFetcher* source,
      const GURL& url,
      const base::Value& json_data,
      bool succeeded);

  CloudPrintURLFetcher::ResponseAction HandlePrinterDeleteResponse(
      const net::URLFetcher* source,
      const GURL& url,
      const base::Value& json_data,
      bool succeeded);

  CloudPrintURLFetcher::ResponseAction HandleRegisterPrinterResponse(
      const net::URLFetcher* source,
      const GURL& url,
      const base::Value& json_data,
      bool succeeded);
  // End response handlers

  // Helper functions for network requests.
  void StartGetRequest(const GURL& url,
                       int max_retries,
                       ResponseHandler handler);
  void StartPostRequest(CloudPrintURLFetcher::RequestType type,
                        const GURL& url,
                        int max_retries,
                        const std::string& mime_type,
                        const std::string& post_data,
                        ResponseHandler handler);

  // Reports a diagnostic message to the server.
  void ReportUserMessage(const std::string& message_id,
                         const std::string& failure_message);

  bool RemovePrinterFromList(const std::string& printer_name,
                             printing::PrinterList* printer_list);

  void InitJobHandlerForPrinter(const base::Value& printer_data);

  void UpdateSettingsFromPrintersList(const base::Value& json_data);

  void AddPendingAvailableTask();
  void AddPendingDeleteTask(const std::string& id);
  void AddPendingRegisterTask(const printing::PrinterBasicInfo& info);
  void AddPendingTask(const PendingTask& task);
  void ProcessPendingTask();
  void ContinuePendingTaskProcessing();
  void OnPrintersAvailable();
  void OnPrinterRegister(const printing::PrinterBasicInfo& info);
  void OnPrinterDelete(const std::string& name);

  void OnReceivePrinterCaps(
      bool succeeded,
      const std::string& printer_name,
      const printing::PrinterCapsAndDefaults& caps_and_defaults);

  // Register printer from the list.
  void RegisterPrinters(const printing::PrinterList& printers);

  bool IsSamePrinter(const std::string& name1, const std::string& name2) const;
  bool InitPrintSystem();

  void ScheduleStatsReport();
  void ReportStats();

  // CloudPrintConnector client.
  Client* client_;
  // Connector settings.
  ConnectorSettings settings_;
  // Pointer to current print system.
  scoped_refptr<PrintSystem> print_system_;
  // Watcher for print system updates.
  scoped_refptr<PrintSystem::PrintServerWatcher>
      print_server_watcher_;
  // A map of printer id to job handler.
  using JobHandlerMap = std::map<std::string, scoped_refptr<PrinterJobHandler>>;
  JobHandlerMap job_handler_map_;
  // Next response handler.
  ResponseHandler next_response_handler_;
  // The list of pending tasks to be done in the background.
  std::list<PendingTask> pending_tasks_;
  // The CloudPrintURLFetcher instance for the current request.
  scoped_refptr<CloudPrintURLFetcher> request_;
  // The CloudPrintURLFetcher instance for the user message request.
  scoped_refptr<CloudPrintURLFetcher> user_message_request_;
  // Partial network traffic annotation for network requests.
  const net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation_;

  base::WeakPtrFactory<CloudPrintConnector> stats_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CloudPrintConnector);
};

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_CONNECTOR_H_

