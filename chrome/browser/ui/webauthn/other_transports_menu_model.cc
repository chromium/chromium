// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/other_transports_menu_model.h"

#include "base/numerics/safe_conversions.h"
#include "chrome/browser/ui/webauthn/transport_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

gfx::ImageSkia GetTransportIcon(AuthenticatorTransport transport) {
  constexpr int kTransportIconSize = 16;
  return gfx::CreateVectorIcon(*GetTransportVectorIcon(transport),
                               kTransportIconSize, gfx::kGoogleGrey700);
}

}  // namespace

OtherTransportsMenuModel::OtherTransportsMenuModel(
    AuthenticatorRequestDialogModel* dialog_model,
    AuthenticatorTransport current_transport)
    : ui::SimpleMenuModel(this), dialog_model_(dialog_model) {
  DCHECK(dialog_model);
  dialog_model_->AddObserver(this);

#if defined(OS_WIN)
  // During the caBLE dialog, if the native Windows authenticator is available,
  // show a single pseudo transport value for switching to the native Windows
  // option.
  if (dialog_model_->transport_availability()
          ->has_win_native_api_authenticator) {
    DCHECK(current_transport ==
           AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy);
    AppendItemForNativeWinApi();
    return;
  }
#endif  // defined(OS_WIN)

  if (current_transport == AuthenticatorTransport::kBluetoothLowEnergy)
    AppendItemForAnotherBluetoothKey();
  PopulateWithTransportsExceptFor(current_transport);
}

OtherTransportsMenuModel::~OtherTransportsMenuModel() {
  if (dialog_model_) {
    dialog_model_->RemoveObserver(this);
    dialog_model_ = nullptr;
  }
}

void OtherTransportsMenuModel::PopulateWithTransportsExceptFor(
    AuthenticatorTransport current_transport) {
  for (const auto transport :
       dialog_model_->transport_availability()->available_transports) {
    if (transport == current_transport)
      continue;

    auto name = GetTransportHumanReadableName(
        transport, TransportSelectionContext::kOtherTransportsMenu);
    AddItemWithIcon(base::strict_cast<int>(transport), std::move(name),
                    GetTransportIcon(transport));
  }
}

void OtherTransportsMenuModel::AppendItemForAnotherBluetoothKey() {
  AddItemWithIcon(
      base::strict_cast<int>(AuthenticatorTransport::kBluetoothLowEnergy),
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_TRANSPORT_POPUP_ANOTHER_BLE),
      GetTransportIcon(AuthenticatorTransport::kBluetoothLowEnergy));
}

#if defined(OS_WIN)
// Magic command ID for calling AbandonFlowAndDispatchToNativeWindowsApi().
// This must not be a defined AuthenticatorTransport value.
constexpr int kWinNativeApiMenuCommand = 999;

void OtherTransportsMenuModel::AppendItemForNativeWinApi() {
  AddItemWithIcon(
      kWinNativeApiMenuCommand,
      l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_TRANSPORT_POPUP_DIFFERENT_AUTHENTICATOR_WIN),
      GetTransportIcon(AuthenticatorTransport::kUsbHumanInterfaceDevice));
}
#endif  // defined(OS_WIN)

bool OtherTransportsMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool OtherTransportsMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

void OtherTransportsMenuModel::ExecuteCommand(int command_id, int event_flags) {
  DCHECK(dialog_model_);

#if defined(OS_WIN)
  if (command_id == kWinNativeApiMenuCommand) {
    DCHECK(dialog_model_->transport_availability()
               ->has_win_native_api_authenticator);
    dialog_model_->HideDialogAndDispatchToNativeWindowsApi();
    return;
  }
#endif  // defined(OS_WIN)

  AuthenticatorTransport selected_transport =
      static_cast<AuthenticatorTransport>(command_id);

  bool pair_with_new_bluetooth_device = false;
  if (selected_transport == AuthenticatorTransport::kBluetoothLowEnergy &&
      dialog_model_->current_step() ==
          AuthenticatorRequestDialogModel::Step::kBleActivate) {
    pair_with_new_bluetooth_device = true;
  }
  dialog_model_->StartGuidedFlowForTransport(selected_transport,
                                             pair_with_new_bluetooth_device);
}

void OtherTransportsMenuModel::OnModelDestroyed() {
  dialog_model_ = nullptr;
}
