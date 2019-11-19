// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_connector.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/hash/md5.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/common/cloud_print/cloud_print_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/service/cloud_print/cloud_print_service_helpers.h"
#include "net/base/mime_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/l10n/l10n_util.h"

namespace cloud_print {

CloudPrintConnector::CloudPrintConnector(
    Client* client,
    const ConnectorSettings& settings,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
    : client_(client),
      next_response_handler_(NULL),
      partial_traffic_annotation_(partial_traffic_annotation) {
  settings_.CopyFrom(settings);
}

bool CloudPrintConnector::InitPrintSystem() {
  if (print_system_.get())
    return true;
  print_system_ = PrintSystem::CreateInstance(
      settings_.print_system_settings());
  if (!print_system_.get()) {
    NOTREACHED();
    return false;  // No memory.
  }
  PrintSystem::PrintSystemResult result = print_system_->Init();
  if (!result.succeeded()) {
    print_system_.reset();
    // We could not initialize the print system. We need to notify the server.
    ReportUserMessage(kPrintSystemFailedMessageId, result.message());
    return false;
  }
  return true;
}

void CloudPrintConnector::ScheduleStatsReport() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CloudPrintConnector::ReportStats,
                     stats_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromHours(1));
}

void CloudPrintConnector::ReportStats() {
  PrinterJobHandler::ReportsStats();
  ScheduleStatsReport();
}

bool CloudPrintConnector::Start() {
  VLOG(1) << "CP_CONNECTOR: Starting connector"
          << ", proxy id: " << settings_.proxy_id();

  pending_tasks_.clear();

  if (!InitPrintSystem())
    return false;

  ScheduleStatsReport();

  // Start watching for updates from the print system.
  print_server_watcher_ = print_system_->CreatePrintServerWatcher();
  print_server_watcher_->StartWatching(this);

  // Get list of registered printers.
  AddPendingAvailableTask();
  return true;
}

void CloudPrintConnector::Stop() {
  VLOG(1) << "CP_CONNECTOR: Stopping connector"
          << ", proxy id: " << settings_.proxy_id();
  DCHECK(IsRunning());
  // Do uninitialization here.
  stats_ptr_factory_.InvalidateWeakPtrs();
  pending_tasks_.clear();
  print_server_watcher_.reset();
  request_.reset();
}

bool CloudPrintConnector::IsRunning() {
  return print_server_watcher_.get() != NULL;
}

std::list<std::string> CloudPrintConnector::GetPrinterIds() const {
  std::list<std::string> printer_ids;
  for (const auto& it : job_handler_map_)
    printer_ids.push_back(it.first);
  return printer_ids;
}

void CloudPrintConnector::RegisterPrinters(
    const printing::PrinterList& printers) {
  if (!IsRunning())
    return;

  for (const auto& it : printers) {
    if (settings_.ShouldConnect(it.printer_name))
      AddPendingRegisterTask(it);
  }
}

// Check for jobs for specific printer
void CloudPrintConnector::CheckForJobs(const std::string& reason,
                                       const std::string& printer_id) {
  if (!IsRunning())
    return;

  if (printer_id.empty()) {
    for (const auto& it : job_handler_map_)
      it.second->CheckForJobs(reason);
    return;
  }

  auto printer_it = job_handler_map_.find(printer_id);
  if (printer_it == job_handler_map_.end()) {
    std::string status_message =
        l10n_util::GetStringUTF8(IDS_CLOUD_PRINT_ZOMBIE_PRINTER);
    LOG(ERROR) << "CP_CONNECTOR: " << status_message
               << " Printer_id: " << printer_id;
    ReportUserMessage(kZombiePrinterMessageId, status_message);
    return;
  }
  printer_it->second->CheckForJobs(reason);
}

void CloudPrintConnector::UpdatePrinterSettings(const std::string& printer_id) {
  // Since connector is managing many printers we need to go through all of them
  // to select the correct settings.
  GURL printer_list_url = GetUrlForPrinterList(
      settings_.server_url(), settings_.proxy_id());
  StartGetRequest(
      printer_list_url,
      kCloudPrintRegisterMaxRetryCount,
      &CloudPrintConnector::HandlePrinterListResponseSettingsUpdate);
}

void CloudPrintConnector::OnPrinterAdded() {
  AddPendingAvailableTask();
}

void CloudPrintConnector::OnPrinterDeleted(const std::string& printer_id) {
  AddPendingDeleteTask(printer_id);
}

void CloudPrintConnector::OnAuthError() {
  client_->OnAuthFailed();
}

// CloudPrintURLFetcher::Delegate implementation.
CloudPrintURLFetcher::ResponseAction CloudPrintConnector::HandleRawData(
    const net::URLFetcher* source,
    const GURL& url,
    const std::string& data) {
  // If this notification came as a result of user message call, stop it.
  // Otherwise proceed continue processing.
  if (user_message_request_ && user_message_request_->IsSameRequest(source))
    return CloudPrintURLFetcher::STOP_PROCESSING;
  return CloudPrintURLFetcher::CONTINUE_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction CloudPrintConnector::HandleJSONData(
    const net::URLFetcher* source,
    const GURL& url,
    const base::Value& json_data,
    bool succeeded) {
  if (!IsRunning())  // Orphan response. Connector has been stopped already.
    return CloudPrintURLFetcher::STOP_PROCESSING;

  DCHECK(next_response_handler_);
  return (this->*next_response_handler_)(source, url, json_data, succeeded);
}

CloudPrintURLFetcher::ResponseAction CloudPrintConnector::OnRequestAuthError() {
  OnAuthError();
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

std::string CloudPrintConnector::GetAuthHeader() {
  return GetCloudPrintAuthHeaderFromStore();
}

CloudPrintConnector::~CloudPrintConnector() {}

CloudPrintURLFetcher::ResponseAction
CloudPrintConnector::HandlePrinterListResponse(const net::URLFetcher* source,
                                               const GURL& url,
                                               const base::Value& json_data,
                                               bool succeeded) {
  DCHECK(succeeded);
  if (!succeeded)
    return CloudPrintURLFetcher::RETRY_REQUEST;

  UpdateSettingsFromPrintersList(json_data);

  // Now we need to get the list of printers from the print system
  // and split printers into 3 categories:
  // - existing and registered printers
  // - new printers
  // - deleted printers

  // Get list of the printers from the print system.
  printing::PrinterList local_printers;
  PrintSystem::PrintSystemResult result =
      print_system_->EnumeratePrinters(&local_printers);
  bool full_list = result.succeeded();
  if (!full_list) {
    std::string message = result.message();
    if (message.empty())
      message = l10n_util::GetStringFUTF8(IDS_CLOUD_PRINT_ENUM_FAILED,
          l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT));
    // There was a failure enumerating printers. Send a message to the server.
    ReportUserMessage(kEnumPrintersFailedMessageId, message);
  }

  // Go through the list of the cloud printers and init print job handlers.
  // There may be no "printers" value in the JSON
  const base::Value* printer_list =
      json_data.FindKeyOfType(kPrinterListValue, base::Value::Type::LIST);
  if (printer_list) {
    for (const auto& printer : printer_list->GetList()) {
      if (!printer.is_dict())
        continue;

      std::string printer_name;
      std::string printer_id;
      const std::string* str = printer.FindStringKey(kNameValue);
      if (str)
        printer_name = *str;
      str = printer.FindStringKey(kIdValue);
      if (str)
        printer_id = *str;

      if (!settings_.ShouldConnect(printer_name)) {
        VLOG(1) << "CP_CONNECTOR: Deleting " << printer_name
                << " id: " << printer_id << " as blacklisted";
        AddPendingDeleteTask(printer_id);
      } else if (RemovePrinterFromList(printer_name, &local_printers)) {
        InitJobHandlerForPrinter(printer);
      } else {
        // Cloud printer is not found on the local system.
        if (full_list || settings_.delete_on_enum_fail()) {
          // Delete if we get the full list of printers or
          // |delete_on_enum_fail_| is set.
          VLOG(1) << "CP_CONNECTOR: Deleting " << printer_name
                  << " id: " << printer_id << " full_list: " << full_list
                  << " delete_on_enum_fail: "
                  << settings_.delete_on_enum_fail();
          AddPendingDeleteTask(printer_id);
        } else {
          LOG(ERROR) << "CP_CONNECTOR: Printer: " << printer_name
                     << " id: " << printer_id
                     << " not found in print system and full printer list was"
                     << " not received.  Printer will not be able to process"
                     << " jobs.";
        }
      }
    }
  }

  request_ = nullptr;

  RegisterPrinters(local_printers);
  ContinuePendingTaskProcessing();  // Continue processing background tasks.
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintConnector::HandlePrinterListResponseSettingsUpdate(
    const net::URLFetcher* source,
    const GURL& url,
    const base::Value& json_data,
    bool succeeded) {
  DCHECK(succeeded);
  if (!succeeded)
    return CloudPrintURLFetcher::RETRY_REQUEST;

  UpdateSettingsFromPrintersList(json_data);
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintConnector::HandlePrinterDeleteResponse(const net::URLFetcher* source,
                                                 const GURL& url,
                                                 const base::Value& json_data,
                                                 bool succeeded) {
  VLOG(1) << "CP_CONNECTOR: Handler printer delete response"
          << ", succeeded: " << succeeded
          << ", url: " << url;
  ContinuePendingTaskProcessing();  // Continue processing background tasks.
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintConnector::HandleRegisterPrinterResponse(
    const net::URLFetcher* source,
    const GURL& url,
    const base::Value& json_data,
    bool succeeded) {
  VLOG(1) << "CP_CONNECTOR: Handler printer register response"
          << ", succeeded: " << succeeded
          << ", url: " << url;
  if (succeeded) {
    const base::Value* printer_list =
        json_data.FindKeyOfType(kPrinterListValue, base::Value::Type::LIST);
    // There should be a "printers" value in the JSON
    if (printer_list && !printer_list->GetList().empty()) {
      const base::Value& printer_data = printer_list->GetList()[0];
      if (printer_data.is_dict())
        InitJobHandlerForPrinter(printer_data);
    }
  }
  ContinuePendingTaskProcessing();  // Continue processing background tasks.
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

void CloudPrintConnector::StartGetRequest(const GURL& url,
                                          int max_retries,
                                          ResponseHandler handler) {
  next_response_handler_ = handler;
  request_ = CloudPrintURLFetcher::Create(partial_traffic_annotation_);
  request_->StartGetRequest(CloudPrintURLFetcher::REQUEST_UPDATE_JOB,
                            url, this, max_retries, std::string());
}

void CloudPrintConnector::StartPostRequest(
    CloudPrintURLFetcher::RequestType type,
    const GURL& url,
    int max_retries,
    const std::string& mime_type,
    const std::string& post_data,
    ResponseHandler handler) {
  next_response_handler_ = handler;
  request_ = CloudPrintURLFetcher::Create(partial_traffic_annotation_);
  request_->StartPostRequest(
      type, url, this, max_retries, mime_type, post_data, std::string());
}

void CloudPrintConnector::ReportUserMessage(const std::string& message_id,
                                            const std::string& failure_msg) {
  // This is a fire and forget type of function.
  // Result of this request will be ignored.
  std::string mime_boundary = net::GenerateMimeMultipartBoundary();
  GURL url = GetUrlForUserMessage(settings_.server_url(), message_id);
  std::string post_data;
  net::AddMultipartValueForUpload(kMessageTextValue, failure_msg, mime_boundary,
                                  std::string(), &post_data);
  net::AddMultipartFinalDelimiterForUpload(mime_boundary, &post_data);
  std::string mime_type("multipart/form-data; boundary=");
  mime_type += mime_boundary;
  user_message_request_ =
      CloudPrintURLFetcher::Create(partial_traffic_annotation_);
  user_message_request_->StartPostRequest(
      CloudPrintURLFetcher::REQUEST_USER_MESSAGE, url, this, 1, mime_type,
      post_data, std::string());
}

bool CloudPrintConnector::RemovePrinterFromList(
    const std::string& printer_name,
    printing::PrinterList* printer_list) {
  for (auto it = printer_list->begin(); it != printer_list->end(); ++it) {
    if (IsSamePrinter(it->printer_name, printer_name)) {
      printer_list->erase(it);
      return true;
    }
  }
  return false;
}

void CloudPrintConnector::InitJobHandlerForPrinter(
    const base::Value& printer_data) {
  DCHECK(printer_data.is_dict());

  PrinterJobHandler::PrinterInfoFromCloud printer_info_cloud;
  const std::string* str = printer_data.FindStringKey(kIdValue);
  if (str)
    printer_info_cloud.printer_id = *str;
  DCHECK(!printer_info_cloud.printer_id.empty());
  VLOG(1) << "CP_CONNECTOR: Init job handler"
          << ", printer id: " << printer_info_cloud.printer_id;
  if (base::Contains(job_handler_map_, printer_info_cloud.printer_id))
    return;  // Nothing to do if we already have a job handler for this printer.

  printing::PrinterBasicInfo printer_info;
  str = printer_data.FindStringKey(kNameValue);
  if (str)
    printer_info.printer_name = *str;
  DCHECK(!printer_info.printer_name.empty());
  str = printer_data.FindStringKey(kPrinterDescValue);
  if (str)
    printer_info.printer_description = *str;
  // Printer status is a string value which actually contains an integer.
  str = printer_data.FindStringKey(kPrinterStatusValue);
  if (str)
    base::StringToInt(*str, &printer_info.printer_status);
  str = printer_data.FindStringKey(kPrinterCapsHashValue);
  if (str)
    printer_info_cloud.caps_hash = *str;

  const base::Value* tags_list =
      printer_data.FindKeyOfType(kTagsValue, base::Value::Type::LIST);
  if (tags_list) {
    for (const auto& tag : tags_list->GetList()) {
      if (tag.is_string() &&
          base::StartsWith(tag.GetString(), kCloudPrintServiceTagsHashTagName,
                           base::CompareCase::INSENSITIVE_ASCII)) {
        std::vector<std::string> tag_parts = base::SplitString(
            tag.GetString(), "=", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
        if (tag_parts.size() == 2)
          printer_info_cloud.tags_hash = tag_parts[1];
      }
    }
  }

  int xmpp_timeout =
      printer_data.FindIntKey(kLocalSettingsPendingXmppValue).value_or(0);
  printer_info_cloud.current_xmpp_timeout = settings_.xmpp_ping_timeout_sec();
  printer_info_cloud.pending_xmpp_timeout = xmpp_timeout;

  scoped_refptr<PrinterJobHandler> job_handler;
  job_handler = new PrinterJobHandler(printer_info,
                                      printer_info_cloud,
                                      settings_.server_url(),
                                      print_system_.get(),
                                      this);
  job_handler_map_[printer_info_cloud.printer_id] = job_handler;
  job_handler->Initialize();
}

void CloudPrintConnector::UpdateSettingsFromPrintersList(
    const base::Value& json_data) {
  int min_xmpp_timeout = std::numeric_limits<int>::max();
  // There may be no "printers" value in the JSON
  const base::Value* printer_list =
      json_data.FindKeyOfType(kPrinterListValue, base::Value::Type::LIST);
  if (printer_list) {
    for (const auto& printer : printer_list->GetList()) {
      if (printer.is_dict()) {
        int xmpp_timeout = 0;
        base::Optional<int> timeout =
            printer.FindIntKey(kLocalSettingsPendingXmppValue);
        if (timeout) {
          xmpp_timeout = *timeout;
          min_xmpp_timeout = std::min(xmpp_timeout, min_xmpp_timeout);
        }
      }
    }
  }

  if (min_xmpp_timeout != std::numeric_limits<int>::max()) {
    DCHECK(min_xmpp_timeout >= kMinXmppPingTimeoutSecs);
    settings_.SetXmppPingTimeoutSec(min_xmpp_timeout);
    client_->OnXmppPingUpdated(min_xmpp_timeout);
  }
}

void CloudPrintConnector::AddPendingAvailableTask() {
  PendingTask task;
  task.type = PENDING_PRINTERS_AVAILABLE;
  AddPendingTask(task);
}

void CloudPrintConnector::AddPendingDeleteTask(const std::string& id) {
  PendingTask task;
  task.type = PENDING_PRINTER_DELETE;
  task.printer_id = id;
  AddPendingTask(task);
}

void CloudPrintConnector::AddPendingRegisterTask(
    const printing::PrinterBasicInfo& info) {
  PendingTask task;
  task.type = PENDING_PRINTER_REGISTER;
  task.printer_info = info;
  AddPendingTask(task);
}

void CloudPrintConnector::AddPendingTask(const PendingTask& task) {
  pending_tasks_.push_back(task);
  // If this is the only pending task, we need to start the process.
  if (pending_tasks_.size() == 1) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&CloudPrintConnector::ProcessPendingTask, this));
  }
}

void CloudPrintConnector::ProcessPendingTask() {
  if (!IsRunning())
    return;  // Orphan call.

  if (pending_tasks_.empty())
    return;  // No peding tasks.

  PendingTask task = pending_tasks_.front();

  switch (task.type) {
    case PENDING_PRINTERS_AVAILABLE :
      OnPrintersAvailable();
      break;
    case PENDING_PRINTER_REGISTER :
      OnPrinterRegister(task.printer_info);
      break;
    case PENDING_PRINTER_DELETE :
      OnPrinterDelete(task.printer_id);
      break;
    default:
      NOTREACHED();
  }
}

void CloudPrintConnector::ContinuePendingTaskProcessing() {
  if (pending_tasks_.empty())
    return;  // No pending tasks.

  // Delete current task and repost if we have more task available.
  pending_tasks_.pop_front();
  if (pending_tasks_.empty())
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&CloudPrintConnector::ProcessPendingTask, this));
}

void CloudPrintConnector::OnPrintersAvailable() {
  GURL printer_list_url = GetUrlForPrinterList(
      settings_.server_url(), settings_.proxy_id());
  StartGetRequest(printer_list_url,
                  kCloudPrintRegisterMaxRetryCount,
                  &CloudPrintConnector::HandlePrinterListResponse);
}

void CloudPrintConnector::OnPrinterRegister(
    const printing::PrinterBasicInfo& info) {
  for (const auto& it : job_handler_map_) {
    if (IsSamePrinter(it.second->GetPrinterName(), info.printer_name)) {
      // Printer already registered, continue to the next task.
      ContinuePendingTaskProcessing();
      return;
    }
  }

  // Asynchronously fetch the printer caps and defaults. The story will
  // continue in OnReceivePrinterCaps.
  print_system_->GetPrinterCapsAndDefaults(
      info.printer_name.c_str(),
      base::BindOnce(&CloudPrintConnector::OnReceivePrinterCaps,
                     base::Unretained(this)));
}

void CloudPrintConnector::OnPrinterDelete(const std::string& printer_id) {
  // Remove corresponding printer job handler.
  auto it = job_handler_map_.find(printer_id);
  if (it != job_handler_map_.end()) {
    it->second->Shutdown();
    job_handler_map_.erase(it);
  }

  // TODO(gene): We probably should not try indefinitely here. Just once or
  // twice should be enough.
  // Bug: http://code.google.com/p/chromium/issues/detail?id=101850
  GURL url = GetUrlForPrinterDelete(
      settings_.server_url(), printer_id, "printer_deleted");
  StartGetRequest(url,
                  kCloudPrintAPIMaxRetryCount,
                  &CloudPrintConnector::HandlePrinterDeleteResponse);
}

void CloudPrintConnector::OnReceivePrinterCaps(
    bool succeeded,
    const std::string& printer_name,
    const printing::PrinterCapsAndDefaults& caps_and_defaults) {
  if (!IsRunning())
    return;  // Orphan call.

  DCHECK(!pending_tasks_.empty());
  DCHECK_EQ(PENDING_PRINTER_REGISTER, pending_tasks_.front().type);

  if (!succeeded) {
    LOG(ERROR) << "CP_CONNECTOR: Failed to get printer info"
               << ", printer name: " << printer_name;
    // This printer failed to register, notify the server of this failure.
    base::string16 printer_name_utf16 = base::UTF8ToUTF16(printer_name);
    std::string status_message = l10n_util::GetStringFUTF8(
        IDS_CLOUD_PRINT_REGISTER_PRINTER_FAILED,
        printer_name_utf16,
        l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT));
    ReportUserMessage(kGetPrinterCapsFailedMessageId, status_message);

    ContinuePendingTaskProcessing();  // Skip this printer registration.
    return;
  }

  const printing::PrinterBasicInfo& info = pending_tasks_.front().printer_info;
  DCHECK(IsSamePrinter(info.printer_name, printer_name));

  std::string mime_boundary = net::GenerateMimeMultipartBoundary();
  std::string post_data;

  net::AddMultipartValueForUpload(kProxyIdValue,
      settings_.proxy_id(), mime_boundary, std::string(), &post_data);
  net::AddMultipartValueForUpload(kPrinterNameValue,
      info.printer_name, mime_boundary, std::string(), &post_data);
  net::AddMultipartValueForUpload(kPrinterDescValue,
      info.printer_description, mime_boundary, std::string(), &post_data);
  net::AddMultipartValueForUpload(kPrinterStatusValue,
                                  base::NumberToString(info.printer_status),
                                  mime_boundary, std::string(), &post_data);
  // Add local_settings with a current XMPP ping interval.
  net::AddMultipartValueForUpload(kPrinterLocalSettingsValue,
      base::StringPrintf(kCreateLocalSettingsXmppPingFormat,
          settings_.xmpp_ping_timeout_sec()),
      mime_boundary, std::string(), &post_data);
  post_data += GetPostDataForPrinterInfo(info, mime_boundary);
  if (caps_and_defaults.caps_mime_type == kContentTypeJSON) {
    net::AddMultipartValueForUpload(kUseCDD, "true", mime_boundary,
                                    std::string(), &post_data);
  }
  net::AddMultipartValueForUpload(kPrinterCapsValue,
      caps_and_defaults.printer_capabilities, mime_boundary,
      caps_and_defaults.caps_mime_type, &post_data);
  net::AddMultipartValueForUpload(kPrinterDefaultsValue,
      caps_and_defaults.printer_defaults, mime_boundary,
      caps_and_defaults.defaults_mime_type, &post_data);
  // Send a hash of the printer capabilities to the server. We will use this
  // later to check if the capabilities have changed
  net::AddMultipartValueForUpload(kPrinterCapsHashValue,
      base::MD5String(caps_and_defaults.printer_capabilities),
      mime_boundary, std::string(), &post_data);
  net::AddMultipartFinalDelimiterForUpload(mime_boundary, &post_data);
  std::string mime_type("multipart/form-data; boundary=");
  mime_type += mime_boundary;

  GURL post_url = GetUrlForPrinterRegistration(settings_.server_url());
  StartPostRequest(CloudPrintURLFetcher::REQUEST_REGISTER, post_url,
                   kCloudPrintAPIMaxRetryCount, mime_type, post_data,
                   &CloudPrintConnector::HandleRegisterPrinterResponse);
}

bool CloudPrintConnector::IsSamePrinter(const std::string& name1,
                                        const std::string& name2) const {
  return base::EqualsCaseInsensitiveASCII(name1, name2);
}

}  // namespace cloud_print
