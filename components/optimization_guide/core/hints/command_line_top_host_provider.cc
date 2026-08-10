// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/hints/command_line_top_host_provider.h"

#include <optional>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"

namespace optimization_guide {

namespace {

// Parses a list of hosts to have hints fetched for. This overrides scheduling
// of the first hints fetch and forces it to occur immediately. If no hosts are
// provided, nullopt is returned.
std::optional<std::vector<std::string>>
ParseHintsFetchOverrideFromCommandLine() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(kFetchHintsOverrideSwitch)) {
    return std::nullopt;
  }

  std::string override_hosts_value =
      cmd_line->GetSwitchValueASCII(kFetchHintsOverrideSwitch);

  std::vector<std::string> hosts =
      base::SplitString(override_hosts_value, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  if (hosts.empty()) {
    return std::nullopt;
  }

  return hosts;
}

}  // namespace

// static
std::unique_ptr<CommandLineTopHostProvider>
CommandLineTopHostProvider::CreateIfEnabled() {
  std::optional<std::vector<std::string>> top_hosts =
      ParseHintsFetchOverrideFromCommandLine();
  if (top_hosts) {
    // Note: wrap_unique is used because the constructor is private.
    return base::WrapUnique(new CommandLineTopHostProvider(*top_hosts));
  }

  return nullptr;
}

CommandLineTopHostProvider::CommandLineTopHostProvider(
    const std::vector<std::string>& top_hosts)
    : top_hosts_(top_hosts) {}

CommandLineTopHostProvider::~CommandLineTopHostProvider() = default;

std::vector<std::string> CommandLineTopHostProvider::GetTopHosts() {
  return top_hosts_;
}

}  // namespace optimization_guide
