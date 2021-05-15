// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/logger_impl.h"

#include <memory>

#include "base/values.h"
#include "components/download/internal/background_service/driver_entry.h"
#include "components/download/internal/background_service/entry.h"
#include "components/download/internal/background_service/log_source.h"
#include "components/download/public/background_service/clients.h"
#include "components/download/public/background_service/download_params.h"

namespace download {
namespace {

std::string ControllerStateToString(Controller::State state) {
  switch (state) {
    case Controller::State::CREATED:
      return "CREATED";
    case Controller::State::INITIALIZING:
      return "INITIALIZING";
    case Controller::State::READY:
      return "READY";
    case Controller::State::RECOVERING:
      return "RECOVERING";
    case Controller::State::UNAVAILABLE:  // Intentional fallthrough.
    default:
      return "UNAVAILABLE";
  }
}

std::string OptBoolToString(absl::optional<bool> value) {
  if (value.has_value())
    return value.value() ? "OK" : "BAD";

  return "UNKNOWN";
}

std::string ClientToString(DownloadClient client) {
  switch (client) {
    case DownloadClient::TEST:
    case DownloadClient::TEST_2:
    case DownloadClient::TEST_3:
    case DownloadClient::INVALID:
      return "TEST";
    case DownloadClient::OFFLINE_PAGE_PREFETCH:
      return "OfflinePagePrefetch";
    case DownloadClient::BACKGROUND_FETCH:
      return "BackgroundFetch";
    case DownloadClient::DEBUGGING:
      return "Debugging";
    case DownloadClient::MOUNTAIN_INTERNAL:
      return "MountainInternal";
    case DownloadClient::PLUGIN_VM_IMAGE:
      return "PluginVmImage";
    case DownloadClient::OPTIMIZATION_GUIDE_PREDICTION_MODELS:
      return "OptimizationGuidePredictionModels";
    case DownloadClient::BOUNDARY:  // Intentional fallthrough.
    default:
      NOTREACHED();
      return std::string();
  }
}

std::string EntryStateToString(Entry::State state) {
  switch (state) {
    case Entry::State::NEW:
      return "NEW";
    case Entry::State::AVAILABLE:
      return "AVAILABLE";
    case Entry::State::ACTIVE:
      return "ACTIVE";
    case Entry::State::PAUSED:
      return "PAUSED";
    case Entry::State::COMPLETE:
      return "COMPLETE";
    default:
      NOTREACHED();
      return std::string();
  }
}

std::string DriverEntryStateToString(DriverEntry::State state) {
  switch (state) {
    case DriverEntry::State::IN_PROGRESS:
      return "IN_PROGRESS";
    case DriverEntry::State::COMPLETE:
      return "COMPLETE";
    case DriverEntry::State::CANCELLED:
      return "CANCELLED";
    case DriverEntry::State::INTERRUPTED:
      return "INTERRUPTED";
    default:
      NOTREACHED();
      return std::string();
  }
}

std::string CompletionTypeToString(CompletionType type) {
  switch (type) {
    case CompletionType::SUCCEED:
      return "SUCCEED";
    case CompletionType::FAIL:
      return "FAIL";
    case CompletionType::ABORT:
      return "ABORT";
    case CompletionType::TIMEOUT:
      return "TIMEOUT";
    case CompletionType::UNKNOWN:
      return "UNKNOWN";
    case CompletionType::CANCEL:
      return "CANCEL";
    case CompletionType::OUT_OF_RETRIES:
      return "OUT_OF_RETRIES";
    case CompletionType::OUT_OF_RESUMPTIONS:
      return "OUT_OF_RESUMPTIONS";
    default:
      NOTREACHED();
      return std::string();
  }
}

std::string StartResultToString(DownloadParams::StartResult result) {
  switch (result) {
    case DownloadParams::StartResult::ACCEPTED:
      return "ACCEPTED";
    case DownloadParams::StartResult::BACKOFF:
      return "BACKOFF";
    case DownloadParams::StartResult::UNEXPECTED_CLIENT:
      return "UNEXPECTED_CLIENT";
    case DownloadParams::StartResult::UNEXPECTED_GUID:
      return "UNEXPECTED_GUID";
    case DownloadParams::StartResult::CLIENT_CANCELLED:
      return "CLIENT_CANCELLED";
    case DownloadParams::StartResult::INTERNAL_ERROR:
      return "INTERNAL_ERROR";
    default:
      NOTREACHED();
      return std::string();
  }
}

std::unique_ptr<base::DictionaryValue> DriverEntryToValue(
    const DriverEntry& entry) {
  std::unique_ptr<base::DictionaryValue> serialized_entry =
      std::make_unique<base::DictionaryValue>();
  serialized_entry->SetString("state", DriverEntryStateToString(entry.state));
  serialized_entry->SetBoolean("paused", entry.paused);
  serialized_entry->SetBoolean("done", entry.done);
  return serialized_entry;
}

std::unique_ptr<base::DictionaryValue> EntryToValue(
    const Entry& entry,
    const absl::optional<DriverEntry>& driver,
    const absl::optional<CompletionType>& completion_type) {
  std::unique_ptr<base::DictionaryValue> serialized_entry =
      std::make_unique<base::DictionaryValue>();
  serialized_entry->SetString("client", ClientToString(entry.client));
  serialized_entry->SetString("state", EntryStateToString(entry.state));
  serialized_entry->SetString("guid", entry.guid);

  // Convert the URL to a proper logging format.
  GURL::Replacements replacements;
  replacements.ClearQuery();

  serialized_entry->SetString(
      "url", entry.request_params.url.ReplaceComponents(replacements).spec());
  serialized_entry->SetString("file_path",
                              entry.target_file_path.MaybeAsASCII());

  if (driver.has_value()) {
    serialized_entry->SetDouble("bytes_downloaded", driver->bytes_downloaded);
    serialized_entry->SetDictionary("driver",
                                    DriverEntryToValue(driver.value()));
  } else {
    serialized_entry->SetDouble("bytes_downloaded", entry.bytes_downloaded);
  }

  if (completion_type.has_value()) {
    serialized_entry->SetString(
        "result", CompletionTypeToString(completion_type.value()));
  } else if (entry.state == Entry::State::COMPLETE) {
    serialized_entry->SetString(
        "result", CompletionTypeToString(CompletionType::SUCCEED));
  }
  return serialized_entry;
}

}  // namespace

LoggerImpl::LoggerImpl() : log_source_(nullptr) {}
LoggerImpl::~LoggerImpl() = default;

void LoggerImpl::SetLogSource(LogSource* log_source) {
  log_source_ = log_source;
}

void LoggerImpl::AddObserver(Observer* observer) {
  DCHECK(!observers_.HasObserver(observer));
  observers_.AddObserver(observer);
}

void LoggerImpl::RemoveObserver(Observer* observer) {
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

base::Value LoggerImpl::GetServiceStatus() {
  base::DictionaryValue service_status;

  if (!log_source_)
    return std::move(service_status);

  Controller::State state = log_source_->GetControllerState();
  const StartupStatus& status = log_source_->GetStartupStatus();

  service_status.SetString("serviceState", ControllerStateToString(state));
  service_status.SetString("modelStatus", OptBoolToString(status.model_ok));
  service_status.SetString("driverStatus", OptBoolToString(status.driver_ok));
  service_status.SetString("fileMonitorStatus",
                           OptBoolToString(status.file_monitor_ok));

  return std::move(service_status);
}

base::Value LoggerImpl::GetServiceDownloads() {
  base::ListValue serialized_entries;

  if (!log_source_)
    return std::move(serialized_entries);

  auto entries = log_source_->GetServiceDownloads();
  for (auto& entry : entries) {
    serialized_entries.Append(
        EntryToValue(*entry.first, entry.second, absl::nullopt));
  }

  return std::move(serialized_entries);
}

void LoggerImpl::OnServiceStatusChanged() {
  if (observers_.empty())
    return;

  base::Value service_status = GetServiceStatus();

  for (auto& observer : observers_)
    observer.OnServiceStatusChanged(service_status);
}

void LoggerImpl::OnServiceDownloadsAvailable() {
  if (observers_.empty())
    return;

  base::Value service_downloads = GetServiceDownloads();
  for (auto& observer : observers_)
    observer.OnServiceDownloadsAvailable(service_downloads);
}

void LoggerImpl::OnServiceDownloadChanged(const std::string& guid) {
  if (observers_.empty())
    return;

  auto entry_details = log_source_->GetServiceDownload(guid);
  if (!entry_details.has_value())
    return;

  auto entry = EntryToValue(*(entry_details->first), entry_details->second,
                            absl::nullopt);

  for (auto& observer : observers_)
    observer.OnServiceDownloadChanged(*entry);
}

void LoggerImpl::OnServiceDownloadFailed(CompletionType completion_type,
                                         const Entry& entry) {
  DCHECK_NE(CompletionType::SUCCEED, completion_type);

  if (observers_.empty())
    return;

  auto serialized_entry = EntryToValue(entry, absl::nullopt, completion_type);
  for (auto& observer : observers_)
    observer.OnServiceDownloadFailed(*serialized_entry);
}

void LoggerImpl::OnServiceRequestMade(
    DownloadClient client,
    const std::string& guid,
    DownloadParams::StartResult start_result) {
  if (observers_.empty())
    return;

  base::DictionaryValue serialized_request;
  serialized_request.SetString("client", ClientToString(client));
  serialized_request.SetString("guid", guid);
  serialized_request.SetString("result", StartResultToString(start_result));
  for (auto& observer : observers_)
    observer.OnServiceRequestMade(serialized_request);
}

}  // namespace download
