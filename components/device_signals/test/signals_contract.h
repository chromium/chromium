// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_TEST_SIGNALS_CONTRACT_H_
#define COMPONENTS_DEVICE_SIGNALS_TEST_SIGNALS_CONTRACT_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"

namespace device_signals::test {

// Returns a map from a signal name to a predicate which evaluates an
// expectation based on a given signals dictionary.
// This represents the inline flow contract.
base::flat_map<std::string,
               base::RepeatingCallback<bool(const base::Value::Dict&)>>
GetSignalsContract();

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns a map from a signal name to a predicate which evaluates an
// expectation based on a given signals dictionary.
// This represents the inline flow contract for unmanaged devices (crOS).
base::flat_map<std::string,
               base::RepeatingCallback<bool(const base::Value::Dict&)>>
GetSignalsContractForUnmanagedDevices();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace device_signals::test

#endif  // COMPONENTS_DEVICE_SIGNALS_TEST_SIGNALS_CONTRACT_H_
