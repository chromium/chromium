// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_PERFORMANCE_LOGGER_H_
#define CHROME_TEST_CHROMEDRIVER_PERFORMANCE_LOGGER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/command_listener.h"

class Log;
struct Session;

// Translates DevTools profiler events into Log messages with info level.
//
// The message is a JSON string of the following structure:
// {
//    "webview": <originating WebView ID>,
//    "message": { "method": "...", "params": { ... }}  // DevTools message.
// }
//
// Also translates buffered trace events into Log messages of info level with
// the same structure if tracing categories are specified.

class PerformanceLogger : public DevToolsEventListener, public CommandListener {
 public:
  // Creates a |PerformanceLogger| with default preferences that creates entries
  // in the given Log object. The log is owned elsewhere and must not be null.
  PerformanceLogger(Log* log, const Session* session);

  // Creates a |PerformanceLogger| with specific preferences.
  PerformanceLogger(Log* log,
                    const Session* session,
                    const PerfLoggingPrefs& prefs,
                    bool enable_service_worker = false);

  PerformanceLogger(const PerformanceLogger&) = delete;
  PerformanceLogger& operator=(const PerformanceLogger&) = delete;

  // PerformanceLogger subscribes to browser-wide |DevToolsClient| for tracing.
  bool subscribes_to_browser() override;

  // For browser-wide client: enables tracing if trace categories are specified,
  // sets |browser_client_|. For other clients: calls EnableInspectorDomains.
  Status OnConnected(DevToolsClient* client) override;

  // Calls HandleInspectorEvents or HandleTraceEvents depending on client type.
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override;

  // Before allowed commands, if tracing enabled, calls CollectTraceEvents.
  Status BeforeCommand(const std::string& command_name) override;

 private:
  void AddLogEntry(Log::Level level,
                   const std::string& webview,
                   const std::string& method,
                   const base::Value::Dict& params);

  void AddLogEntry(const std::string& webview,
                   const std::string& method,
                   const base::Value::Dict& params);

  // Enables Network and Page domains according to |PerfLoggingPrefs|.
  Status EnableInspectorDomains(DevToolsClient* client);

  // Logs Network and Page events.
  Status HandleInspectorEvents(DevToolsClient* client,
                               const std::string& method,
                               const base::Value::Dict& params);

  // Logs trace events and monitors trace buffer usage.
  Status HandleTraceEvents(DevToolsClient* client,
                           const std::string& method,
                           const base::Value::Dict& params);

  bool ShouldReportTracingError();
  Status StartTrace();  // Must not call before browser-wide client connects.
  Status CollectTraceEvents();  // Ditto.
  Status IsTraceDone(bool* trace_done) const; // True if trace is not buffering.

  raw_ptr<Log> log_;  // The log where to create entries.
  raw_ptr<const Session> session_;
  PerfLoggingPrefs prefs_;
  raw_ptr<DevToolsClient>
      browser_client_;    // Pointer to browser-wide |DevToolsClient|.
  bool trace_buffering_;  // True unless trace stopped and all events received.
  bool enable_service_worker_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_PERFORMANCE_LOGGER_H_
