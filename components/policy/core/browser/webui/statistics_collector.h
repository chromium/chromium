// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_WEBUI_STATISTICS_COLLECTOR_H_
#define COMPONENTS_POLICY_CORE_BROWSER_WEBUI_STATISTICS_COLLECTOR_H_

#include <stdint.h>

#include "components/policy/policy_export.h"

namespace policy {

POLICY_EXPORT void RecordPolicyUIButtonUsage(uint32_t reload_policies_count,
                                             uint32_t export_to_json_count,
                                             uint32_t copy_to_json_count,
                                             uint32_t upload_report_count);
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_WEBUI_STATISTICS_COLLECTOR_H_
