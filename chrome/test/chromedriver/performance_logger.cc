// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/performance_logger.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "chrome/test/chromedriver/session.h"

namespace {

// DevTools event domain prefixes to intercept.
const char* const kDomains[] = {"Network.", "Page."};

// Whitelist of WebDriver commands on which to request buffered trace events.
const char* const kRequestTraceCommands[] = {"GetLog" /* required */,
    "Navigate"};

bool IsBrowserwideClient(DevToolsClient* client) {
  return (client->GetId() == DevToolsClientImpl::kBrowserwideDevToolsClientId);
}

bool IsEnabled(const PerfLoggingPrefs::InspectorDomainStatus& domain_status) {
    return (domain_status ==
            PerfLoggingPrefs::InspectorDomainStatus::kDefaultEnabled ||
            domain_status ==
            PerfLoggingPrefs::InspectorDomainStatus::kExplicitlyEnabled);
}

// Returns whether |command| is in kRequestTraceCommands[] (case-insensitive).
// In the case of GetLog, also check if it has been called previously, that it
// was emptying an empty log.
bool ShouldRequestTraceEvents(const std::string& command,
                              const bool log_emptied) {
  if (base::EqualsCaseInsensitiveASCII(command, "GetLog") && !log_emptied)
    return false;

  for (auto* request_command : kRequestTraceCommands) {
    if (base::EqualsCaseInsensitiveASCII(command, request_command))
      return true;
  }
  return false;
}

// Returns whether the event belongs to one of kDomains.
bool ShouldLogEvent(const std::string& method) {
  for (auto* domain : kDomains) {
    if (base::StartsWith(method, domain, base::CompareCase::SENSITIVE))
      return true;
  }
  return false;
}

}  // namespace

PerformanceLogger::PerformanceLogger(Log* log, const Session* session)
    : log_(log),
      session_(session),
      browser_client_(nullptr),
      trace_buffering_(false),
      enable_service_worker_(false) {}

PerformanceLogger::PerformanceLogger(Log* log,
                                     const Session* session,
                                     const PerfLoggingPrefs& prefs,
                                     bool enable_service_worker)
    : log_(log),
      session_(session),
      prefs_(prefs),
      browser_client_(nullptr),
      trace_buffering_(false),
      enable_service_worker_(enable_service_worker) {}

bool PerformanceLogger::subscribes_to_browser() {
  return true;
}

Status PerformanceLogger::OnConnected(DevToolsClient* client) {
  if (IsBrowserwideClient(client)) {
    browser_client_ = client;
    if (prefs_.trace_categories.empty())
      return Status(kOk);
    return StartTrace();
  }
  return EnableInspectorDomains(client);
}

Status PerformanceLogger::OnEvent(DevToolsClient* client,
                                  const std::string& method,
                                  const base::Value::Dict& params) {
  if (method == "Target.attachedToTarget") {
    const std::string* type = params.FindStringByDottedPath("targetInfo.type");
    if (!type) {
      return Status(kUnknownError,
                    "missing target type in Target.attachedToTarget event");
    }
    if (enable_service_worker_ && *type == "service_worker") {
      const std::string* target_id =
          params.FindStringByDottedPath("targetInfo.targetId");
      if (!target_id) {
        return Status(kUnknownError,
                      "missing target ID in Target.attachedToTarget event");
      }

      std::list<std::string> webview_ids;
      Status status = session_->chrome->GetWebViewIds(&webview_ids,
                                                      session_->w3c_compliant);
      if (status.IsError())
        return status;

      WebView* webview = nullptr;
      status = session_->chrome->GetWebViewById(*target_id, &webview);
      if (status.IsError())
        return status;
    }
  }
  if (IsBrowserwideClient(client)) {
    return HandleTraceEvents(client, method, params);
  } else {
    return HandleInspectorEvents(client, method, params);
  }
}

Status PerformanceLogger::BeforeCommand(const std::string& command_name) {
  // Only dump trace buffer after tracing has been started.
  if (trace_buffering_ &&
      ShouldRequestTraceEvents(command_name, log_->Emptied())) {
    Status status = CollectTraceEvents();
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

void PerformanceLogger::AddLogEntry(Log::Level level,
                                    const std::string& webview,
                                    const std::string& method,
                                    const base::Value::Dict& params) {
  base::Value::Dict log_message_dict;
  log_message_dict.Set("webview", webview);
  log_message_dict.SetByDottedPath("message.method", method);
  log_message_dict.SetByDottedPath("message.params", params.Clone());
  std::string log_message_json;
  base::JSONWriter::Write(log_message_dict, &log_message_json);

  // TODO(klm): extract timestamp from params?
  // Look at where it is for Page, Network, and trace events.
  log_->AddEntry(level, log_message_json);
}

void PerformanceLogger::AddLogEntry(const std::string& webview,
                                    const std::string& method,
                                    const base::Value::Dict& params) {
  AddLogEntry(Log::kInfo, webview, method, params);
}

Status PerformanceLogger::EnableInspectorDomains(DevToolsClient* client) {
  std::vector<std::string> enable_commands;
  if (IsEnabled(prefs_.network)) {
    enable_commands.push_back("Network.enable");
  }
  for (const auto& enable_command : enable_commands) {
    base::Value::Dict params;  // All the enable commands have empty params.
    Status status = client->SendCommand(enable_command, params);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

Status PerformanceLogger::HandleInspectorEvents(
    DevToolsClient* client,
    const std::string& method,
    const base::Value::Dict& params) {
  if (!ShouldLogEvent(method))
    return Status(kOk);

  AddLogEntry(client->GetId(), method, params);
  return Status(kOk);
}

Status PerformanceLogger::HandleTraceEvents(DevToolsClient* client,
                                            const std::string& method,
                                            const base::Value::Dict& params) {
  if (method == "Tracing.tracingComplete") {
    trace_buffering_ = false;
  } else if (method == "Tracing.dataCollected") {
    // The Tracing.dataCollected event contains a list of trace events.
    // Add each one as an individual log entry of method Tracing.dataCollected.
    const base::Value::List* traces = params.FindList("value");
    if (!traces) {
      return Status(kUnknownError,
                    "received DevTools trace data in unexpected format");
    }
    for (const auto& trace : *traces) {
      const base::Value::Dict* event_dict = trace.GetIfDict();
      if (!event_dict)
        return Status(kUnknownError, "trace event must be a dictionary");
      AddLogEntry(client->GetId(), "Tracing.dataCollected", *event_dict);
    }
  } else if (method == "Tracing.bufferUsage") {
    // 'value' will be between 0-1 and represents how full the DevTools trace
    // buffer is. If the buffer is full, warn the user.
    std::optional<double> maybe_buffer_usage = params.FindDouble("percentFull");
    if (!maybe_buffer_usage.has_value()) {
      // Tracing.bufferUsage event will occur once per second, and it really
      // only serves as a warning, so if we can't reliably tell whether the
      // buffer is full, just fail silently instead of spamming the logs.
      return Status(kOk);
    }
    if (maybe_buffer_usage.value() >= 0.99999) {
      base::Value::Dict error_params;
      std::string err("Chrome's trace buffer filled while collecting events, "
                      "so some trace events may have been lost");
      error_params.Set("error", err);
      // Expose error to client via perf log using same format as other entries.
      AddLogEntry(Log::kWarning,
                  DevToolsClientImpl::kBrowserwideDevToolsClientId,
                  "Tracing.bufferUsage", error_params);
      LOG(WARNING) << err;
    }
  }
  return Status(kOk);
}

Status PerformanceLogger::StartTrace() {
  if (!browser_client_) {
    return Status(kUnknownError, "tried to start tracing, but connection to "
                  "browser was not yet established");
  }
  if (trace_buffering_) {
    LOG(WARNING) << "tried to start tracing, but a trace was already started";
    return Status(kOk);
  }
  base::Value::List categories;
  const std::vector<std::string> str_list =
      base::SplitString(prefs_.trace_categories, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const std::string& str : str_list) {
    categories.Append(str);
  }
  base::Value::Dict params;
  params.SetByDottedPath("traceConfig.includedCategories",
                         std::move(categories));
  params.SetByDottedPath("traceConfig.recordingMode", "recordAsMuchAsPossible");
  // Ask DevTools to report buffer usage.
  params.Set("bufferUsageReportingInterval",
             prefs_.buffer_usage_reporting_interval);
  Status status = browser_client_->SendCommand("Tracing.start", params);
  if (status.IsError()) {
    LOG(ERROR) << "error when starting trace: " << status.message();
    return status;
  }
  trace_buffering_ = true;
  return Status(kOk);
}

Status PerformanceLogger::CollectTraceEvents() {
  if (!browser_client_) {
    return Status(kUnknownError, "tried to collect trace events, but "
                  "connection to browser was not yet established");
  }
  if (!trace_buffering_) {
    return Status(kUnknownError, "tried to collect trace events, but tracing "
                  "was not started");
  }

  base::Value::Dict params;
  Status status = browser_client_->SendCommand("Tracing.end", params);
  if (status.IsError()) {
    LOG(ERROR) << "error when stopping trace: " << status.message();
    return status;
  }

  // Block up to 30 seconds until Tracing.tracingComplete event is received.
  status = browser_client_->HandleEventsUntil(
      base::BindRepeating(&PerformanceLogger::IsTraceDone,
                          base::Unretained(this)),
      Timeout(base::Seconds(30)));
  if (status.IsError())
    return status;

  return StartTrace();
}

Status PerformanceLogger::IsTraceDone(bool* trace_done) const {
  *trace_done = !trace_buffering_;
  return Status(kOk);
}
