// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_SIGNALS_SIGNALS_FILTERER_H_
#define COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_SIGNALS_SIGNALS_FILTERER_H_

#include "base/values.h"

namespace enterprise_connectors {

// This class is in charge of removing or modifying signals from the signal
// payload, e.g., for privacy reasons.
class SignalsFilterer {
 public:
  virtual ~SignalsFilterer() = default;

  // Modifies `signals` based on the current device context.
  virtual void Filter(base::DictValue& signals);
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_SIGNALS_SIGNALS_FILTERER_H_
