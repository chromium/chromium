// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_handler.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/bluetooth/debug_logs_manager.h"
#include "device/bluetooth/adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/bluetooth/debug_logs_manager.h"
#endif

BluetoothInternalsHandler::BluetoothInternalsHandler(
    mojo::PendingReceiver<mojom::BluetoothInternalsHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

BluetoothInternalsHandler::~BluetoothInternalsHandler() = default;

void BluetoothInternalsHandler::GetAdapter(GetAdapterCallback callback) {
  if (device::BluetoothAdapterFactory::IsBluetoothSupported()) {
    device::BluetoothAdapterFactory::GetAdapter(
        base::BindOnce(&BluetoothInternalsHandler::OnGetAdapter,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    std::move(callback).Run(mojo::NullRemote() /* adapter */);
  }
}

void BluetoothInternalsHandler::GetDebugLogsChangeHandler(
    GetDebugLogsChangeHandlerCallback callback) {
  mojom::DebugLogsChangeHandlerPtr handler_ptr;
  bool initial_toggle_value = false;

#if defined(OS_CHROMEOS)
  using chromeos::bluetooth::DebugLogsManager;

  // If no logs manager exists for this user, debug logs are not supported.
  DebugLogsManager::DebugLogsState state =
      debug_logs_manager_ ? debug_logs_manager_->GetDebugLogsState()
                          : DebugLogsManager::DebugLogsState::kNotSupported;

  switch (state) {
    case DebugLogsManager::DebugLogsState::kNotSupported:
      // Leave |handler_ptr| null and |initial_toggle_value| false.
      break;
    case DebugLogsManager::DebugLogsState::kSupportedAndEnabled:
      initial_toggle_value = true;
      FALLTHROUGH;
    case DebugLogsManager::DebugLogsState::kSupportedButDisabled:
      handler_ptr = debug_logs_manager_->GenerateInterfacePtr();
      break;
  }
#endif

  std::move(callback).Run(std::move(handler_ptr), initial_toggle_value);
}

void BluetoothInternalsHandler::OnGetAdapter(
    GetAdapterCallback callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  mojo::PendingRemote<bluetooth::mojom::Adapter> pending_adapter;
  mojo::MakeSelfOwnedReceiver(std::make_unique<bluetooth::Adapter>(adapter),
                              pending_adapter.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(std::move(pending_adapter));
}
