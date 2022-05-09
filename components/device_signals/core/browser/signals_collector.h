// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_COLLECTOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_COLLECTOR_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"

namespace base {
class Value;
}  // namespace base

namespace device_signals {

class SignalsCollector {
 public:
  using GetSignalCallback = base::OnceCallback<void(base::Value)>;

  virtual ~SignalsCollector() = default;

  // Returns the array of signal names that this collector can collect.
  virtual const std::vector<std::string> GetSupportedSignalNames() = 0;

  // Collects the signal named `signal_name` using `params` (if needed), and
  // invokes `callback` with the signal value.
  virtual void GetSignal(const std::string& signal_name,
                         const base::Value& params,
                         GetSignalCallback callback) = 0;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_COLLECTOR_H_
