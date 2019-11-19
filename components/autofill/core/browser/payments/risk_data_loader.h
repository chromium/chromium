// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_RISK_DATA_LOADER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_RISK_DATA_LOADER_H_

#include <string>

#include "base/callback.h"

namespace autofill {

class RiskDataLoader {
 public:
  // Gathers risk data and provides it to |callback|.
  virtual void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) = 0;

 protected:
  virtual ~RiskDataLoader() {}
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_RISK_DATA_LOADER_H_
