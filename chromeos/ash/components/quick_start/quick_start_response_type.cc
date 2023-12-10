// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/quick_start/quick_start_response_type.h"

namespace ash::quick_start {

std::ostream& operator<<(std::ostream& stream,
                         const QuickStartResponseType& response_type) {
  switch (response_type) {
    case QuickStartResponseType::kHandshake:
      stream << "Handshake";
      break;
    case QuickStartResponseType::kWifiCredentials:
      stream << "WifiCredentials";
      break;
    case QuickStartResponseType::kNotifySourceOfUpdate:
      stream << "NotifySourceOfUpdate";
      break;
    case QuickStartResponseType::kBootstrapConfigurations:
      stream << "BootstrapConfigurations";
      break;
    case QuickStartResponseType::kGetInfo:
      stream << "GetInfo";
      break;
    case QuickStartResponseType::kAssertion:
      stream << "Assertion";
      break;
    case QuickStartResponseType::kBootstrapStateCancel:
      stream << "BootstrapStateCancel";
      break;
    case QuickStartResponseType::kBootstrapStateComplete:
      stream << "BootstrapStateComplete";
      break;
  }
  return stream;
}

}  // namespace ash::quick_start
