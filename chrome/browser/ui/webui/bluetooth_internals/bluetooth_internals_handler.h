// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BLUETOOTH_INTERNALS_BLUETOOTH_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_BLUETOOTH_INTERNALS_BLUETOOTH_INTERNALS_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace ash {
namespace bluetooth {
class DebugLogsManager;
}  // namespace bluetooth
}  // namespace ash
#endif

// Handles API requests from chrome://bluetooth-internals page by implementing
// mojom::BluetoothInternalsHandler.
class BluetoothInternalsHandler : public mojom::BluetoothInternalsHandler {
 public:
  explicit BluetoothInternalsHandler(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::BluetoothInternalsHandler> receiver);

  BluetoothInternalsHandler(const BluetoothInternalsHandler&) = delete;
  BluetoothInternalsHandler& operator=(const BluetoothInternalsHandler&) =
      delete;

  ~BluetoothInternalsHandler() override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void set_debug_logs_manager(
      ash::bluetooth::DebugLogsManager* debug_logs_manager) {
    debug_logs_manager_ = debug_logs_manager;
  }
#endif

  // mojom::BluetoothInternalsHandler:
  void GetAdapter(GetAdapterCallback callback) override;
  void GetDebugLogsChangeHandler(
      GetDebugLogsChangeHandlerCallback callback) override;
  void CheckSystemPermissions(CheckSystemPermissionsCallback callback) override;
  void RequestSystemPermissions(
      RequestSystemPermissionsCallback callback) override;
  void RequestLocationServices(
      RequestLocationServicesCallback callback) override;

 private:
  void OnGetAdapter(GetAdapterCallback callback,
                    scoped_refptr<device::BluetoothAdapter> adapter);

  raw_ref<content::RenderFrameHost> render_frame_host_;
  mojo::Receiver<mojom::BluetoothInternalsHandler> receiver_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::bluetooth::DebugLogsManager* debug_logs_manager_ = nullptr;
#endif

  base::WeakPtrFactory<BluetoothInternalsHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_BLUETOOTH_INTERNALS_BLUETOOTH_INTERNALS_HANDLER_H_
