// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_COMMAND_LINE_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_COMMAND_LINE_H_

#include <memory>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
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

  std::unique_ptr<PolicyBundle> Load();

 private:
  const base::CommandLine& command_line_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_COMMAND_LINE_H_
