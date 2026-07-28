// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_COMMON_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_COMMON_H_

namespace browser_actuator {

// Identifies feature payload targets, matching the server
// DownstreamMessage.PayloadType.
// TODO(crbug.com/532660606): Update this to map to the enum once it is synced
// under components/optimization_guide/proto/.
enum class PayloadType {
  kUnspecified = 0,
};

// Identifies the feature factory instance, to ensure we do not re-register
// factories of the same type.
enum class FactoryId {
  kUnset = 0,
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_COMMON_H_
