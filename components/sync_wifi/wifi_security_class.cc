// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_wifi/wifi_security_class.h"

#include "base/logging.h"
#include "components/onc/onc_constants.h"

namespace sync_wifi {

bool WifiSecurityClassSupportsPassphrases(
    const WifiSecurityClass security_class) {
  switch (security_class) {
    case SECURITY_CLASS_NONE:
      return false;
    case SECURITY_CLASS_WEP:
    case SECURITY_CLASS_PSK:
    case SECURITY_CLASS_802_1X:
      return true;
    case SECURITY_CLASS_INVALID:
      return false;
  }
  NOTREACHED() << "Invalid WifiSecurityClass enum with value "
               << security_class;
  return false;
}

WifiSecurityClass WifiSecurityClassFromSyncSecurityClass(
    const sync_pb::WifiCredentialSpecifics_SecurityClass sync_enum) {
  switch (sync_enum) {
    case sync_pb::WifiCredentialSpecifics_SecurityClass_SECURITY_CLASS_INVALID:
      return WifiSecurityClass::SECURITY_CLASS_INVALID;
    case sync_pb::WifiCredentialSpecifics_SecurityClass_SECURITY_CLASS_NONE:
      return WifiSecurityClass::SECURITY_CLASS_NONE;
    case sync_pb::WifiCredentialSpecifics_SecurityClass_SECURITY_CLASS_WEP:
      return WifiSecurityClass::SECURITY_CLASS_WEP;
    case sync_pb::WifiCredentialSpecifics_SecurityClass_SECURITY_CLASS_PSK:
      return WifiSecurityClass::SECURITY_CLASS_PSK;
  }
  NOTREACHED() << "Invalid sync security class enum with value " << sync_enum;
  return WifiSecurityClass::SECURITY_CLASS_INVALID;
}

sync_pb::WifiCredentialSpecifics_SecurityClass
WifiSecurityClassToSyncSecurityClass(const WifiSecurityClass security_class) {
  switch (security_class) {
    case SECURITY_CLASS_NONE:
      return sync_pb::WifiCredentialSpecifics::SECURITY_CLASS_NONE;
    case SECURITY_CLASS_WEP:
      return sync_pb::WifiCredentialSpecifics::SECURITY_CLASS_WEP;
    case SECURITY_CLASS_PSK:
      return sync_pb::WifiCredentialSpecifics::SECURITY_CLASS_PSK;
    case SECURITY_CLASS_802_1X:
      LOG(WARNING) << "Unsupported security class 802.1X";
      return sync_pb::WifiCredentialSpecifics::SECURITY_CLASS_INVALID;
    case SECURITY_CLASS_INVALID:
      return sync_pb::WifiCredentialSpecifics::SECURITY_CLASS_INVALID;
  }
  NOTREACHED() << "Invalid WifiSecurityClass enum with value "
               << security_class;
  return sync_pb::WifiCredentialSpecifics::SECURITY_CLASS_INVALID;
}

bool WifiSecurityClassToOncSecurityString(WifiSecurityClass security_class,
                                          std::string* security_class_string) {
  DCHECK(security_class_string);
  switch (security_class) {
    case SECURITY_CLASS_NONE:
      *security_class_string = onc::wifi::kSecurityNone;
      return true;
    case SECURITY_CLASS_WEP:
      *security_class_string = onc::wifi::kWEP_PSK;
      return true;
    case SECURITY_CLASS_PSK:
      *security_class_string = onc::wifi::kWPA_PSK;
      return true;
    case SECURITY_CLASS_802_1X:
      *security_class_string = onc::wifi::kWPA_EAP;
      return true;
    case SECURITY_CLASS_INVALID:
      return false;
  }
  NOTREACHED() << "Invalid WifiSecurityClass enum with value "
               << security_class;
  return false;
}

}  // namespace sync_wifi
