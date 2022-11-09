// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_command_line.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_types.h"

namespace policy {

PolicyLoaderCommandLine::PolicyLoaderCommandLine(
    const base::CommandLine& command_line)
    : command_line_(command_line) {}
PolicyLoaderCommandLine::~PolicyLoaderCommandLine() = default;

PolicyBundle PolicyLoaderCommandLine::Load() {
  PolicyBundle bundle;
  if (!command_line_->HasSwitch(switches::kChromePolicy))
    return bundle;

  auto policies = base::JSONReader::ReadAndReturnValueWithError(
      command_line_->GetSwitchValueASCII(switches::kChromePolicy),
      base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  if (!policies.has_value()) {
    VLOG(1) << "Command line policy error: " << policies.error().message;
    return bundle;
  }
  if (!policies->is_dict()) {
    VLOG(1) << "Command line policy is not a dictionary";
    return bundle;
  }

  bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .LoadFrom(policies->GetDict(), POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_MACHINE, POLICY_SOURCE_COMMAND_LINE);
  return bundle;
}

}  // namespace policy
