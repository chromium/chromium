// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting/process_extractor.h"

#include <string>
#include <utility>

namespace enterprise_connectors {

ProcessNode::ProcessNode() = default;

ProcessNode::ProcessNode(base::ProcessId process_id,
                         base::ProcessId parent_process_id,
                         std::string executable_path,
                         std::string script_path)
    : process_id(process_id),
      parent_process_id(parent_process_id),
      executable_path(std::move(executable_path)),
      script_path(std::move(script_path)) {}

ProcessNode::ProcessNode(const ProcessNode&) = default;
ProcessNode& ProcessNode::operator=(const ProcessNode&) = default;
ProcessNode::ProcessNode(ProcessNode&&) noexcept = default;
ProcessNode& ProcessNode::operator=(ProcessNode&&) noexcept = default;
ProcessNode::~ProcessNode() = default;

}  // namespace enterprise_connectors
