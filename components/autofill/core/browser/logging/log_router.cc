// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/logging/log_router.h"

// TODO(crbug.com/1380255): Remove the next two lines
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/observer_list.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/common/logging/log_buffer.h"

namespace autofill {

LogRouter::LogRouter() = default;

// TODO(crbug.com/1380255): Turn this back to ~LogRouter() = default;
LogRouter::~LogRouter() {
  if (!managers_.empty() || !receivers_.empty()) {
    SCOPED_CRASH_KEY_STRING32("autofill::LogRouter", "managers_",
                              managers_.empty() ? "empty" : "not empty");
    SCOPED_CRASH_KEY_STRING32("autofill::LogRouter", "receivers_",
                              receivers_.empty() ? "empty" : "not empty");
    base::debug::DumpWithoutCrashing();
  }
}

// static
base::Value::Dict LogRouter::CreateEntryForText(const std::string& text) {
  LogBuffer buffer(LogBuffer::IsActive(true));
  buffer << Tag{"div"};
  for (const auto& line : base::SplitStringPiece(
           text, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    buffer << line << Br{};
  }
  buffer << CTag{};
  return *buffer.RetrieveResult();
}

void LogRouter::ProcessLog(const std::string& text) {
  ProcessLog(CreateEntryForText(text));
}

void LogRouter::ProcessLog(const base::Value::Dict& node) {
  // This may not be called when there are no receivers (i.e., the router is
  // inactive), because in that case the logs cannot be displayed.
  DCHECK(!receivers_.empty());
  for (LogReceiver& receiver : receivers_)
    receiver.LogEntry(node);
}

bool LogRouter::RegisterManager(RoutingLogManager* manager) {
  DCHECK(manager);
  managers_.AddObserver(manager);
  return !receivers_.empty();
}

void LogRouter::UnregisterManager(RoutingLogManager* manager) {
  DCHECK(managers_.HasObserver(manager));
  managers_.RemoveObserver(manager);
}

void LogRouter::RegisterReceiver(LogReceiver* receiver) {
  DCHECK(receiver);
  if (receivers_.empty()) {
    for (RoutingLogManager& manager : managers_)
      manager.OnLogRouterAvailabilityChanged(true);
  }
  receivers_.AddObserver(receiver);
}

void LogRouter::UnregisterReceiver(LogReceiver* receiver) {
  DCHECK(receivers_.HasObserver(receiver));
  receivers_.RemoveObserver(receiver);
  if (receivers_.empty()) {
    for (RoutingLogManager& manager : managers_)
      manager.OnLogRouterAvailabilityChanged(false);
  }
}

}  // namespace autofill
