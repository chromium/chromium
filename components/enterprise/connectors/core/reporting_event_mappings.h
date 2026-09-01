// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_EVENT_MAPPINGS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_EVENT_MAPPINGS_H_

#include <string>

#include "components/enterprise/common/proto/synced_from_google3/chrome_reporting_entity.pb.h"

namespace enterprise_connectors {

// Alias to reduce verbosity when using Event::EventCase.
using EventCase = ::chrome::cros::reporting::proto::Event::EventCase;

std::string GetPayloadSizeUmaMetricName(EventCase event_case);

std::string GetEventName(EventCase event_case);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_EVENT_MAPPINGS_H_
