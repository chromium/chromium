// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/trace_to_console.h"

#include <string>

#include "base/command_line.h"
#include "components/tracing/common/tracing_switches.h"

namespace tracing {

namespace {
// These categories will cause deadlock when ECHO_TO_CONSOLE. crbug.com/325575.
const char kEchoToConsoleCategoryFilter[] = "-ipc,-toplevel";
}  // namespace

base::trace_event::TraceConfig GetConfigForTraceToConsole() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  DCHECK(command_line.HasSwitch(switches::kTraceToConsole));
  std::string filter = command_line.GetSwitchValueASCII(
      switches::kTraceToConsole);
  if (filter.empty()) {
    filter = kEchoToConsoleCategoryFilter;
  } else {
    filter.append(",");
    filter.append(kEchoToConsoleCategoryFilter);
  }
  return base::trace_event::TraceConfig(
      filter, base::trace_event::ECHO_TO_CONSOLE);
}

}  // namespace tracing
