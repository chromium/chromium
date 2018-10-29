// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/trace_event_args_whitelist.h"

#include "base/bind.h"
#include "base/strings/pattern.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"

namespace {

struct WhitelistEntry {
  const char* category_name;
  const char* event_name;
  const char* const* arg_name_filter;
};

const char* const kGPUAllowedArgs[] = {nullptr};
const char* const kInputLatencyAllowedArgs[] = {"data", nullptr};
const char* const kMemoryDumpAllowedArgs[] = {"dumps", nullptr};

const WhitelistEntry kEventArgsWhitelist[] = {
    {"__metadata", "thread_name", nullptr},
    {"__metadata", "process_name", nullptr},
    {"__metadata", "process_uptime_seconds", nullptr},
    {"__metadata", "chrome_library_address", nullptr},
    {"__metadata", "chrome_library_module", nullptr},
    {"__metadata", "stackFrames", nullptr},
    {"__metadata", "typeNames", nullptr},
    {"GPU", "*", kGPUAllowedArgs},
    {"ipc", "GpuChannelHost::Send", nullptr},
    {"ipc", "SyncChannel::Send", nullptr},
    {"latencyInfo", "*", kInputLatencyAllowedArgs},
    {"shutdown", "*", nullptr},
    {"task_scheduler", "*", nullptr},
    {"toplevel", "*", nullptr},
    {TRACE_DISABLED_BY_DEFAULT("cpu_profiler"), "*", nullptr},
    // Redefined the string since MemoryDumpManager::kTraceCategory causes
    // static initialization of this struct.
    {TRACE_DISABLED_BY_DEFAULT("memory-infra"), "*", kMemoryDumpAllowedArgs},
    {TRACE_DISABLED_BY_DEFAULT("system_stats"), "*", nullptr},
    {nullptr, nullptr, nullptr}};

const char* kMetadataWhitelist[] = {"chrome-library-name",
                                    "clock-domain",
                                    "config",
                                    "cpu-*",
                                    "field-trials",
                                    "gpu-*",
                                    "highres-ticks",
                                    "last_triggered_rule",
                                    "network-type",
                                    "num-cpus",
                                    "os-*",
                                    "physical-memory",
                                    "product-version",
                                    "scenario_name",
                                    "trace-config",
                                    "user-agent"};

}  // namespace

bool IsTraceArgumentNameWhitelisted(const char* const* granular_filter,
                                    const char* arg_name) {
  for (int i = 0; granular_filter[i] != nullptr; ++i) {
    if (base::MatchPattern(arg_name, granular_filter[i]))
      return true;
  }

  return false;
}

bool IsTraceEventArgsWhitelisted(
    const char* category_group_name,
    const char* event_name,
    base::trace_event::ArgumentNameFilterPredicate* arg_name_filter) {
  DCHECK(arg_name_filter);
  base::CStringTokenizer category_group_tokens(
      category_group_name, category_group_name + strlen(category_group_name),
      ",");
  while (category_group_tokens.GetNext()) {
    const std::string& category_group_token = category_group_tokens.token();
    for (int i = 0; kEventArgsWhitelist[i].category_name != nullptr; ++i) {
      const WhitelistEntry& whitelist_entry = kEventArgsWhitelist[i];
      DCHECK(whitelist_entry.event_name);

      if (base::MatchPattern(category_group_token,
                             whitelist_entry.category_name) &&
          base::MatchPattern(event_name, whitelist_entry.event_name)) {
        if (whitelist_entry.arg_name_filter) {
          *arg_name_filter = base::Bind(&IsTraceArgumentNameWhitelisted,
                                        whitelist_entry.arg_name_filter);
        }
        return true;
      }
    }
  }

  return false;
}

bool IsMetadataWhitelisted(const std::string& metadata_name) {
  for (auto* key : kMetadataWhitelist) {
    if (base::MatchPattern(metadata_name, key)) {
      return true;
    }
  }
  return false;
}
