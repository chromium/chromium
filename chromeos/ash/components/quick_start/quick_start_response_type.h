// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_RESPONSE_TYPE_H_
#define CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_RESPONSE_TYPE_H_

#include <ostream>

namespace ash::quick_start {

enum class QuickStartResponseType {
  kHandshake,
  kWifiCredentials,
  kNotifySourceOfUpdate,
  kBootstrapConfigurations,
  kGetInfo,
  kAssertion,
  kBootstrapStateCancel,
  kBootstrapStateComplete,
};

std::ostream& operator<<(std::ostream& stream,
                         const QuickStartResponseType& response_type);

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_QUICK_START_QUICK_START_RESPONSE_TYPE_H_
