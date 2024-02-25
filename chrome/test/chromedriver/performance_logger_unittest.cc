// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/performance_logger.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "chrome/test/chromedriver/session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct DevToolsCommand {
  DevToolsCommand(const std::string& in_method, base::Value::Dict* in_params)
      : method(in_method) {
    params.reset(in_params);
  }
  ~DevToolsCommand() = default;

  std::string method;
  std::unique_ptr<base::Value::Dict> params;
};

class FakeDevToolsClient : public StubDevToolsClient {
 public:
  explicit FakeDevToolsClient(const std::string& id) : id_(id) {}
  ~FakeDevToolsClient() override = default;

  bool PopSentCommand(DevToolsCommand** out_command) {
    if (sent_commands_.size() > command_index_) {
      *out_command = sent_commands_[command_index_++].get();
      return true;
    }
    return false;
  }

  Status TriggerEvent(const std::string& method,
                      const base::Value::Dict& params) {
    return listener_->OnEvent(this, method, params);
  }

  Status TriggerEvent(const std::string& method) {
    return TriggerEvent(method, base::Value::Dict());
  }

  Status SendCommandAndGetResult(const std::string& method,
                                 const base::Value::Dict& params,
                                 base::Value::Dict* result) override {
    auto dict = std::make_unique<base::Value::Dict>(params.Clone());
    sent_commands_.push_back(
        std::make_unique<DevToolsCommand>(method, dict.release()));
    return Status(kOk);
  }

  void AddListener(DevToolsEventListener* listener) override {
    CHECK(!listener_);
    listener_ = listener;
  }

  void RemoveListener(DevToolsEventListener* listener) override {
    CHECK(listener_ = listener);
    listener_ = nullptr;
  }

  const std::string& GetId() override { return id_; }

 private:
  const std::string id_;  // WebView id.
  std::vector<std::unique_ptr<DevToolsCommand>>
      sent_commands_;                // Commands that were sent.
  raw_ptr<DevToolsEventListener> listener_ =
      nullptr;  // The fake allows only one event listener.
  size_t command_index_ = 0;
};

struct LogEntry {
  const base::Time timestamp;
  const Log::Level level;
  const std::string source;
  const std::string message;

  LogEntry(const base::Time& timestamp,
           Log::Level level,
           const std::string& source,
           const std::string& message)
      : timestamp(timestamp), level(level), source(source), message(message) {}
};

class FakeLog : public Log {
 public:
  void AddEntryTimestamped(const base::Time& timestamp,
                           Level level,
                           const std::string& source,
                           const std::string& message) override;

  bool Emptied() const override;

  const std::vector<std::unique_ptr<LogEntry>>& GetEntries() {
    return entries_;
  }

 private:
  std::vector<std::unique_ptr<LogEntry>> entries_;
};

void FakeLog::AddEntryTimestamped(const base::Time& timestamp,
                                  Level level,
                                  const std::string& source,
                                  const std::string& message) {
  entries_.push_back(
      std::make_unique<LogEntry>(timestamp, level, source, message));
}

bool FakeLog::Emptied() const {
  return true;
}

base::expected<base::Value::Dict, std::string> ParseDictionary(
    const std::string& json) {
  ASSIGN_OR_RETURN(
      auto parsed_json, base::JSONReader::ReadAndReturnValueWithError(json),
      [&](base::JSONReader::Error error) {
        return "Couldn't parse " + json + ", got: " + std::move(error).message;
      });

  base::Value::Dict* dict = parsed_json.GetIfDict();
  if (!dict) {
    return base::unexpected("JSON object is not a dictionary");
  }

  return std::move(*dict);
}

void ValidateLogEntry(const LogEntry* entry,
                      const std::string& expected_webview,
                      const std::string& expected_method,
                      const base::Value::Dict& expected_params) {
  EXPECT_EQ(Log::kInfo, entry->level);
  EXPECT_LT(0, entry->timestamp.ToTimeT());

  ASSERT_OK_AND_ASSIGN(base::Value::Dict message,
                       ParseDictionary(entry->message));
  const std::string* webview = message.FindString("webview");
  ASSERT_TRUE(webview);
  EXPECT_EQ(expected_webview, *webview);
  const std::string* method = message.FindStringByDottedPath("message.method");
  ASSERT_TRUE(method);
  EXPECT_EQ(expected_method, *method);

  base::Value::Dict* params = message.FindDictByDottedPath("message.params");
  ASSERT_TRUE(params);
  EXPECT_EQ(expected_params, *params);
}

void ValidateLogEntry(const LogEntry *entry,
                      const std::string& expected_webview,
                      const std::string& expected_method) {
  base::Value::Dict empty_params;
  ValidateLogEntry(entry, expected_webview, expected_method, empty_params);
}

void ExpectCommand(FakeDevToolsClient* client, const std::string& method) {
  DevToolsCommand* cmd;
  // Use ASSERT so that test fails if no command is returned.
  ASSERT_TRUE(client->PopSentCommand(&cmd));
  EXPECT_EQ(method, cmd->method);
}

void ExpectEnableDomains(FakeDevToolsClient* client) {
  ExpectCommand(client, "Network.enable");
}

}  // namespace

TEST(PerformanceLogger, OneWebView) {
  FakeDevToolsClient client("webview-1");
  FakeLog log;
  Session session("test");
  PerformanceLogger logger(&log, &session);

  client.AddListener(&logger);
  logger.OnConnected(&client);
  ExpectEnableDomains(&client);
  ASSERT_EQ(kOk, client.TriggerEvent("Network.gaga").code());
  ASSERT_EQ(kOk, client.TriggerEvent("Page.ulala").code());
  // Ignore -- different domain.
  ASSERT_EQ(kOk, client.TriggerEvent("Console.bad").code());

  ASSERT_EQ(2u, log.GetEntries().size());
  ValidateLogEntry(log.GetEntries()[0].get(), "webview-1", "Network.gaga");
  ValidateLogEntry(log.GetEntries()[1].get(), "webview-1", "Page.ulala");
  client.RemoveListener(&logger);
}

TEST(PerformanceLogger, TwoWebViews) {
  FakeDevToolsClient client1("webview-1");
  FakeDevToolsClient client2("webview-2");
  FakeLog log;
  Session session("test");
  PerformanceLogger logger(&log, &session);

  client1.AddListener(&logger);
  client2.AddListener(&logger);
  logger.OnConnected(&client1);
  logger.OnConnected(&client2);
  ExpectEnableDomains(&client1);
  ExpectEnableDomains(&client2);
  // OnConnected sends the enable command only to that client, not others.
  logger.OnConnected(&client1);
  ExpectEnableDomains(&client1);
  DevToolsCommand* cmd;
  ASSERT_FALSE(client2.PopSentCommand(&cmd));

  ASSERT_EQ(kOk, client1.TriggerEvent("Page.gaga1").code());
  ASSERT_EQ(kOk, client2.TriggerEvent("Network.gaga2").code());

  ASSERT_EQ(2u, log.GetEntries().size());
  ValidateLogEntry(log.GetEntries()[0].get(), "webview-1", "Page.gaga1");
  ValidateLogEntry(log.GetEntries()[1].get(), "webview-2", "Network.gaga2");
  client1.RemoveListener(&logger);
  client2.RemoveListener(&logger);
}

TEST(PerformanceLogger, PerfLoggingPrefs) {
  FakeDevToolsClient client("webview-1");
  FakeLog log;
  Session session("test");
  PerfLoggingPrefs prefs;
  EXPECT_EQ(PerfLoggingPrefs::InspectorDomainStatus::kDefaultEnabled,
            prefs.network);
  prefs.network = PerfLoggingPrefs::InspectorDomainStatus::kExplicitlyDisabled;
  prefs.trace_categories = "benchmark,blink.console";
  PerformanceLogger logger(&log, &session, prefs);

  client.AddListener(&logger);
  logger.OnConnected(&client);

  DevToolsCommand* cmd;
  ASSERT_FALSE(client.PopSentCommand(&cmd));
  client.RemoveListener(&logger);
}

namespace {

class FakeBrowserwideClient : public FakeDevToolsClient {
 public:
  FakeBrowserwideClient()
      : FakeDevToolsClient(DevToolsClientImpl::kBrowserwideDevToolsClientId) {}
  ~FakeBrowserwideClient() override = default;

  bool events_handled() const {
    return events_handled_;
  }

  // Overridden from DevToolsClient:
  Status HandleEventsUntil(const ConditionalFunc& conditional_func,
                           const Timeout& timeout) override {
    TriggerEvent("Tracing.tracingComplete");
    events_handled_ = true;
    return Status(kOk);
  }

 private:
  bool events_handled_ = false;
};

}  // namespace

TEST(PerformanceLogger, TracingStartStop) {
  FakeBrowserwideClient client;
  FakeLog log;
  Session session("test");
  PerfLoggingPrefs prefs;
  prefs.trace_categories = "benchmark,blink.console";
  PerformanceLogger logger(&log, &session, prefs);

  client.AddListener(&logger);
  logger.OnConnected(&client);
  DevToolsCommand* cmd;
  ASSERT_TRUE(client.PopSentCommand(&cmd));
  EXPECT_EQ("Tracing.start", cmd->method);
  const base::Value::List* categories =
      cmd->params->FindListByDottedPath("traceConfig.includedCategories");
  ASSERT_TRUE(categories);
  ASSERT_EQ(2u, categories->size());
  ASSERT_TRUE((*categories)[0].is_string());
  EXPECT_EQ("benchmark", (*categories)[0].GetString());
  ASSERT_TRUE((*categories)[1].is_string());
  EXPECT_EQ("blink.console", (*categories)[1].GetString());
  int expected_interval =
      cmd->params->FindInt("bufferUsageReportingInterval").value_or(-1);
  EXPECT_GT(expected_interval, 0);
  ASSERT_FALSE(client.PopSentCommand(&cmd));

  EXPECT_FALSE(client.events_handled());
  // Trigger a dump of the DevTools trace buffer.
  ASSERT_EQ(kOk, logger.BeforeCommand("GetLog").code());
  EXPECT_TRUE(client.events_handled());
  ExpectCommand(&client, "Tracing.end");
  ExpectCommand(&client, "Tracing.start");  // Tracing should re-start.
  ASSERT_FALSE(client.PopSentCommand(&cmd));
  client.RemoveListener(&logger);
}

TEST(PerformanceLogger, RecordTraceEvents) {
  FakeBrowserwideClient client;
  FakeLog log;
  Session session("test");
  PerfLoggingPrefs prefs;
  prefs.trace_categories = "benchmark,blink.console";
  PerformanceLogger logger(&log, &session, prefs);

  client.AddListener(&logger);
  logger.OnConnected(&client);
  base::Value::Dict params;
  base::Value::List trace_events;
  base::Value::Dict event1;
  event1.Set("cat", "foo");
  trace_events.Append(event1.Clone());
  base::Value::Dict event2;
  event2.Set("cat", "bar");
  trace_events.Append(event2.Clone());
  params.Set("value", std::move(trace_events));
  ASSERT_EQ(kOk, client.TriggerEvent("Tracing.dataCollected", params).code());

  ASSERT_EQ(2u, log.GetEntries().size());
  ValidateLogEntry(log.GetEntries()[0].get(),
                   DevToolsClientImpl::kBrowserwideDevToolsClientId,
                   "Tracing.dataCollected", event1);
  ValidateLogEntry(log.GetEntries()[1].get(),
                   DevToolsClientImpl::kBrowserwideDevToolsClientId,
                   "Tracing.dataCollected", event2);
  client.RemoveListener(&logger);
}

TEST(PerformanceLogger, ShouldRequestTraceEvents) {
  FakeBrowserwideClient client;
  FakeLog log;
  Session session("test");
  PerfLoggingPrefs prefs;
  prefs.trace_categories = "benchmark,blink.console";
  PerformanceLogger logger(&log, &session, prefs);

  client.AddListener(&logger);
  logger.OnConnected(&client);
  EXPECT_FALSE(client.events_handled());
  // Trace events should not be dumped for commands not in whitelist.
  ASSERT_EQ(kOk, logger.BeforeCommand("Blah").code());
  EXPECT_FALSE(client.events_handled());
  ASSERT_EQ(kOk, logger.BeforeCommand("Foo").code());
  EXPECT_FALSE(client.events_handled());
  // Trace events should always be dumped for GetLog command.
  ASSERT_EQ(kOk, logger.BeforeCommand("GetLog").code());
  EXPECT_TRUE(client.events_handled());
  client.RemoveListener(&logger);
}

TEST(PerformanceLogger, WarnWhenTraceBufferFull) {
  FakeBrowserwideClient client;
  FakeLog log;
  Session session("test");
  PerfLoggingPrefs prefs;
  prefs.trace_categories = "benchmark,blink.console";
  PerformanceLogger logger(&log, &session, prefs);

  client.AddListener(&logger);
  logger.OnConnected(&client);
  base::Value::Dict params;
  params.Set("percentFull", 1.0);
  ASSERT_EQ(kOk, client.TriggerEvent("Tracing.bufferUsage", params).code());

  ASSERT_EQ(1u, log.GetEntries().size());
  LogEntry* entry = log.GetEntries()[0].get();
  EXPECT_EQ(Log::kWarning, entry->level);
  EXPECT_LT(0, entry->timestamp.ToTimeT());
  ASSERT_OK_AND_ASSIGN(base::Value::Dict message,
                       ParseDictionary(entry->message));
  const std::string* webview = message.FindString("webview");
  ASSERT_TRUE(webview);
  EXPECT_EQ(DevToolsClientImpl::kBrowserwideDevToolsClientId, *webview);
  const std::string* method = message.FindStringByDottedPath("message.method");
  ASSERT_TRUE(method);
  EXPECT_EQ("Tracing.bufferUsage", *method);
  const base::Value::Dict* actual_params =
      message.FindDictByDottedPath("message.params");
  ASSERT_TRUE(actual_params);
  EXPECT_TRUE(actual_params->contains("error"));
  client.RemoveListener(&logger);
}
