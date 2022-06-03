// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/hermes/hermes_response_status.h"

namespace chromeos {

HermesResponseStatus HermesResponseStatusFromErrorName(
    const std::string& error_name) {
  if (error_name == "org.chromium.Hermes.Error.AlreadyDisabled") {
    return HermesResponseStatus::kErrorAlreadyDisabled;
  }
  if (error_name == "org.chromium.Hermes.Error.AlreadyEnabled") {
    return HermesResponseStatus::kErrorAlreadyEnabled;
  }
  if (error_name == "org.chromium.Hermes.Error.InvalidActivationCode") {
    return HermesResponseStatus::kErrorInvalidActivationCode;
  }
  if (error_name == "org.chromium.Hermes.Error.InvalidIccid") {
    return HermesResponseStatus::kErrorInvalidIccid;
  }
  if (error_name == "org.chromium.Hermes.Error.InvalidParameter") {
    return HermesResponseStatus::kErrorInvalidParameter;
  }
  if (error_name == "org.chromium.Hermes.Error.NeedConfirmationCode") {
    return HermesResponseStatus::kErrorNeedConfirmationCode;
  }
  if (error_name == "org.chromium.Hermes.Error.SendNotificationFailure") {
    return HermesResponseStatus::kErrorSendNotificationFailure;
  }
  if (error_name == "org.chromium.Hermes.Error.TestProfileInProd") {
    return HermesResponseStatus::kErrorTestProfileInProd;
  }
  if (error_name == "org.chromium.Hermes.Error.Unsupported") {
    return HermesResponseStatus::kErrorUnsupported;
  }
  if (error_name == "org.chromium.Hermes.Error.WrongState") {
    return HermesResponseStatus::kErrorWrongState;
  }

  return HermesResponseStatus::kErrorUnknown;
}

}  // namespace chromeos
