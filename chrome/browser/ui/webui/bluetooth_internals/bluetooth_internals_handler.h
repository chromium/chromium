// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BLUETOOTH_INTERNALS_BLUETOOTH_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_BLUETOOTH_INTERNALS_BLUETOOTH_INTERNALS_HANDLER_H_

#include <optional>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace ash {
namespace bluetooth {
class DebugLogsManager;
}  // namespace bluetooth
}  // namespace ash
#endif

// Handles API requests from chrome://bluetooth-internals page by implementing
// mojom::BluetoothInternalsHandler.
class BluetoothInternalsHandler
    : public mojom::BluetoothInternalsHandler
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ,
      public ash::bluetooth_config::mojom::SystemPropertiesObserver
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
{
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void RestartSystemBluetooth(RestartSystemBluetoothCallback callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  void StartBtsnoop(StartBtsnoopCallback callback) override;
  void IsBtsnoopFeatureEnabled(
      IsBtsnoopFeatureEnabledCallback callback) override;

 private:
  void OnGetAdapter(GetAdapterCallback callback,
                    scoped_refptr<device::BluetoothAdapter> adapter);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // bluetooth_config::mojom::SystemPropertiesObserver
  void OnPropertiesUpdated(
      ash::bluetooth_config::mojom::BluetoothSystemPropertiesPtr properties)
      override;

  void StopBtsnoop(mojom::BluetoothBtsnoop::StopCallback callback);
  void OnStartBtsnoopResp(StartBtsnoopCallback callback, bool success);
  void OnStopBtsnoopResp(mojom::BluetoothBtsnoop::StopCallback callback,
                         bool success);
  std::optional<base::FilePath> GetDownloadsPath();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  raw_ref<content::RenderFrameHost> render_frame_host_;
  mojo::Receiver<mojom::BluetoothInternalsHandler> receiver_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  raw_ptr<ash::bluetooth::DebugLogsManager> debug_logs_manager_ = nullptr;

  bool turning_bluetooth_off_ = false;
  bool turning_bluetooth_on_ = false;
  bool bluetooth_initial_state_ = false;

  RestartSystemBluetoothCallback restart_system_bluetooth_callback_;

  ash::bluetooth_config::mojom::BluetoothSystemState bluetooth_system_state_ =
      ash::bluetooth_config::mojom::BluetoothSystemState::kUnavailable;

  mojo::Receiver<ash::bluetooth_config::mojom::SystemPropertiesObserver>
      cros_system_properties_observer_receiver_{this};

  mojo::Remote<ash::bluetooth_config::mojom::CrosBluetoothConfig>
      remote_cros_bluetooth_config_;

  std::unique_ptr<mojom::BluetoothBtsnoop> btsnoop_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  base::WeakPtrFactory<BluetoothInternalsHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_BLUETOOTH_INTERNALS_BLUETOOTH_INTERNALS_HANDLER_H_
