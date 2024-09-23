// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_ENUMS_H_
#define COMPONENTS_METRICS_STRUCTURED_ENUMS_H_

namespace metrics::structured {

// Specifies the type of identifier attached to an event.
enum class IdType {
  // Events are attached to a per-event (or per-project) id.
  kProjectId = 0,
  // Events are attached to the UMA client_id.
  kUmaId = 1,
  // Events are attached to no id.
  kUnidentified = 2,
};

// Specifies whether an identifier is used different for each profile, or is
// shared for all profiles on a device.
enum class IdScope {
  kPerProfile = 0,
  kPerDevice = 1,
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_ENUMS_H_
