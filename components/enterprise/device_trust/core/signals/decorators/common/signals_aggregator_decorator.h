// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_SIGNALS_DECORATORS_COMMON_SIGNALS_AGGREGATOR_DECORATOR_H_
#define COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_SIGNALS_DECORATORS_COMMON_SIGNALS_AGGREGATOR_DECORATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/enterprise/device_trust/core/signals/decorators/common/signals_decorator.h"

namespace device_signals {
class SignalsAggregator;
struct SignalsAggregationResponse;
}  // namespace device_signals

namespace enterprise_connectors {

enum class DeviceTrustPasswordProtectionTrigger {
  kUnset = 0,
  kOff = 1,
  kPasswordReuse = 2,
  kPhishingReuse = 3,
};

// A SignalsDecorator implementation that bridges the
// device_signals::SignalsAggregator pipeline into the Device Trust
// SignalsService collection flow.
class SignalsAggregatorDecorator : public SignalsDecorator {
 public:
  explicit SignalsAggregatorDecorator(
      device_signals::SignalsAggregator* signals_aggregator);
  ~SignalsAggregatorDecorator() override;

  SignalsAggregatorDecorator(const SignalsAggregatorDecorator&) = delete;
  SignalsAggregatorDecorator& operator=(const SignalsAggregatorDecorator&) =
      delete;

  // SignalsDecorator:
  void Decorate(base::DictValue& signals,
                base::OnceClosure done_closure) override;

 private:
  void OnSignalsAggregated(base::DictValue& signals,
                           base::OnceClosure done_closure,
                           device_signals::SignalsAggregationResponse response);

  const raw_ptr<device_signals::SignalsAggregator> signals_aggregator_;
  base::WeakPtrFactory<SignalsAggregatorDecorator> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_SIGNALS_DECORATORS_COMMON_SIGNALS_AGGREGATOR_DECORATOR_H_
