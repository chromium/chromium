// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSTEM_FACTORY_PING_EMBARGO_CHECK_H_
#define CHROMEOS_SYSTEM_FACTORY_PING_EMBARGO_CHECK_H_

#include "base/component_export.h"
#include "base/time/time.h"

namespace chromeos {
namespace system {

class StatisticsProvider;

// The RLZ embargo end date is considered invalid if it's more than this many
// days in the future.
constexpr base::TimeDelta kRlzEmbargoEndDateGarbageDateThreshold =
    base::TimeDelta::FromDays(14);

enum class FactoryPingEmbargoState {
  // There is no correctly formatted factory ping embargo end date value in
  // VPD.
  kMissingOrMalformed,
  // There is a correctly formatted factory ping embargo end date value in VPD
  // which is too far in the future (indicating that the time source used in
  // the factory to write the embargo end date was not based on a not
  // synchronized clock).
  kInvalid,
  // The embargo period has not passed yet.
  kNotPassed,
  // The embargo period has passed.
  kPassed
};

COMPONENT_EXPORT(CHROMEOS_SYSTEM)
FactoryPingEmbargoState GetFactoryPingEmbargoState(
    StatisticsProvider* statistics_provider);

}  // namespace system
}  // namespace chromeos

#endif  // CHROMEOS_SYSTEM_FACTORY_PING_EMBARGO_CHECK_H_
