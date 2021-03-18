// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/transport_utils.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

int GetMessageIdForTransportOnTransportSelectionSheet(
    AuthenticatorTransport transport) {
  switch (transport) {
    case AuthenticatorTransport::kBluetoothLowEnergy:
      NOTREACHED();
      return 0;
    case AuthenticatorTransport::kNearFieldCommunication:
      NOTREACHED();
      return IDS_WEBAUTHN_TRANSPORT_NFC;
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      return IDS_WEBAUTHN_TRANSPORT_USB;
    case AuthenticatorTransport::kInternal:
      return IDS_WEBAUTHN_TRANSPORT_INTERNAL;
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
      return IDS_WEBAUTHN_TRANSPORT_CABLE;
    case AuthenticatorTransport::kAndroidAccessory:
      return IDS_WEBAUTHN_TRANSPORT_AOA;
  }
  NOTREACHED();
  return 0;
}

int GetMessageIdForTransportOnOtherTransportsPopup(
    AuthenticatorTransport transport) {
  switch (transport) {
    case AuthenticatorTransport::kBluetoothLowEnergy:
      NOTREACHED();
      return 0;
    case AuthenticatorTransport::kNearFieldCommunication:
      NOTREACHED();
      return IDS_WEBAUTHN_TRANSPORT_POPUP_NFC;
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      return IDS_WEBAUTHN_TRANSPORT_POPUP_USB;
    case AuthenticatorTransport::kInternal:
      return IDS_WEBAUTHN_TRANSPORT_POPUP_INTERNAL;
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
      return IDS_WEBAUTHN_TRANSPORT_POPUP_CABLE;
    case AuthenticatorTransport::kAndroidAccessory:
      return IDS_WEBAUTHN_TRANSPORT_POPUP_AOA;
  }
  NOTREACHED();
  return 0;
}

}  // namespace

std::u16string GetTransportHumanReadableName(
    AuthenticatorTransport transport,
    TransportSelectionContext context) {
  int message_id =
      context == TransportSelectionContext::kTransportSelectionSheet
          ? GetMessageIdForTransportOnTransportSelectionSheet(transport)
          : GetMessageIdForTransportOnOtherTransportsPopup(transport);
  DCHECK_NE(message_id, 0);
  return l10n_util::GetStringUTF16(message_id);
}

const gfx::VectorIcon* GetTransportVectorIcon(
    AuthenticatorTransport transport) {
  switch (transport) {
    case AuthenticatorTransport::kBluetoothLowEnergy:
      NOTREACHED();
      return &kFingerprintIcon;
    case AuthenticatorTransport::kNearFieldCommunication:
      NOTREACHED();
      return &kNfcIcon;
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      return &vector_icons::kUsbIcon;
    case AuthenticatorTransport::kInternal:
      return &kLaptopIcon;
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
      return &kSmartphoneIcon;
    case AuthenticatorTransport::kAndroidAccessory:
      return &kUsbCableIcon;
  }
  NOTREACHED();
  return &kFingerprintIcon;
}
