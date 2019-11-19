// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/logging/log_router.h"

#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "net/base/escape.h"

namespace autofill {

LogRouter::LogRouter() = default;

LogRouter::~LogRouter() = default;

// static
base::Value LogRouter::CreateEntryForText(const std::string& text) {
  LogBuffer buffer;
  buffer << Tag{"div"};
  for (const auto& line : base::SplitStringPiece(
           text, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    buffer << line << Br{};
  }
  buffer << CTag{};
  return buffer.RetrieveResult();
}

void LogRouter::ProcessLog(const std::string& text) {
  ProcessLog(CreateEntryForText(text));
}

void LogRouter::ProcessLog(base::Value&& node) {
  // This may not be called when there are no receivers (i.e., the router is
  // inactive), because in that case the logs cannot be displayed.
  DCHECK(receivers_.might_have_observers());
  accumulated_logs_.emplace_back(std::move(node));
  for (LogReceiver& receiver : receivers_)
    receiver.LogEntry(accumulated_logs_.back());
}

bool LogRouter::RegisterManager(LogManager* manager) {
  DCHECK(manager);
  managers_.AddObserver(manager);
  return receivers_.might_have_observers();
}

void LogRouter::UnregisterManager(LogManager* manager) {
  DCHECK(managers_.HasObserver(manager));
  managers_.RemoveObserver(manager);
}

const std::vector<base::Value>& LogRouter::RegisterReceiver(
    LogReceiver* receiver) {
  DCHECK(receiver);
  DCHECK(accumulated_logs_.empty() || receivers_.might_have_observers());

  if (!receivers_.might_have_observers()) {
    for (LogManager& manager : managers_)
      manager.OnLogRouterAvailabilityChanged(true);
  }
  receivers_.AddObserver(receiver);
  return accumulated_logs_;
}

void LogRouter::UnregisterReceiver(LogReceiver* receiver) {
  DCHECK(receivers_.HasObserver(receiver));
  receivers_.RemoveObserver(receiver);
  if (!receivers_.might_have_observers()) {
    // |accumulated_logs_| can become very long; use the swap instead of clear()
    // to ensure that the memory is freed.
    std::vector<base::Value>().swap(accumulated_logs_);
    for (LogManager& manager : managers_)
      manager.OnLogRouterAvailabilityChanged(false);
  }
}

}  // namespace autofill
