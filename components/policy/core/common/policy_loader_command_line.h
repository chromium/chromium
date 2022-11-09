// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_COMMAND_LINE_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_COMMAND_LINE_H_

#include "base/command_line.h"
#include "base/memory/raw_ref.h"
#include "components/policy/policy_export.h"

namespace policy {

class PolicyBundle;

// Loads policy value from command line switch for development and testing
// purposes. It can be only used with strict limitation. For example, on
// Android, the device must be rooted.
class POLICY_EXPORT PolicyLoaderCommandLine {
 public:
  explicit PolicyLoaderCommandLine(const base::CommandLine& command_line);
  PolicyLoaderCommandLine(const PolicyLoaderCommandLine&) = delete;
  PolicyLoaderCommandLine& operator=(const PolicyLoaderCommandLine&) = delete;

  ~PolicyLoaderCommandLine();

  PolicyBundle Load();

 private:
  const raw_ref<const base::CommandLine> command_line_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_COMMAND_LINE_H_
