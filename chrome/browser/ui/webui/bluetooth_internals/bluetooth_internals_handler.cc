// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_handler.h"

#include <string>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/bluetooth/adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/bluetooth_config_service.h"
#include "chrome/browser/ash/bluetooth/debug_logs_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
#include "components/permissions/android/android_permission_util.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {
using content::RenderFrameHost;
using content::WebContents;

#if BUILDFLAG(IS_CHROMEOS_ASH)
using ash::bluetooth_config::mojom::BluetoothSystemState;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}  // namespace

BluetoothInternalsHandler::BluetoothInternalsHandler(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::BluetoothInternalsHandler> receiver)
    : render_frame_host_(*render_frame_host),
      receiver_(this, std::move(receiver)) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::GetBluetoothConfigService(
      remote_cros_bluetooth_config_.BindNewPipeAndPassReceiver());
  remote_cros_bluetooth_config_->ObserveSystemProperties(
      cros_system_properties_observer_receiver_.BindNewPipeAndPassRemote());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

BluetoothInternalsHandler::~BluetoothInternalsHandler() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!turning_bluetooth_off_ && !turning_bluetooth_on_) {
    return;
  }

  remote_cros_bluetooth_config_->SetBluetoothEnabledState(
      bluetooth_initial_state_);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void BluetoothInternalsHandler::GetAdapter(GetAdapterCallback callback) {
  if (device::BluetoothAdapterFactory::IsBluetoothSupported()) {
    device::BluetoothAdapterFactory::Get()->GetAdapter(
        base::BindOnce(&BluetoothInternalsHandler::OnGetAdapter,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    std::move(callback).Run(mojo::NullRemote() /* adapter */);
  }
}

void BluetoothInternalsHandler::GetDebugLogsChangeHandler(
    GetDebugLogsChangeHandlerCallback callback) {
  mojo::PendingRemote<mojom::DebugLogsChangeHandler> handler_remote;
  bool initial_toggle_value = false;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  using ash::bluetooth::DebugLogsManager;

  // If no logs manager exists for this user, debug logs are not supported.
  DebugLogsManager::DebugLogsState state =
      debug_logs_manager_ ? debug_logs_manager_->GetDebugLogsState()
                          : DebugLogsManager::DebugLogsState::kNotSupported;

  switch (state) {
    case DebugLogsManager::DebugLogsState::kNotSupported:
      // Leave |handler_remote| NullRemote and |initial_toggle_value| false.
      break;
    case DebugLogsManager::DebugLogsState::kSupportedAndEnabled:
      initial_toggle_value = true;
      [[fallthrough]];
    case DebugLogsManager::DebugLogsState::kSupportedButDisabled:
      handler_remote = debug_logs_manager_->GenerateRemote();
      break;
  }
#endif

  std::move(callback).Run(std::move(handler_remote), initial_toggle_value);
}

void BluetoothInternalsHandler::OnGetAdapter(
    GetAdapterCallback callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  mojo::PendingRemote<bluetooth::mojom::Adapter> pending_adapter;
  mojo::MakeSelfOwnedReceiver(std::make_unique<bluetooth::Adapter>(adapter),
                              pending_adapter.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(std::move(pending_adapter));
}

void BluetoothInternalsHandler::CheckSystemPermissions(
    CheckSystemPermissionsCallback callback) {
  bool need_location_permission = false;
  bool need_nearby_devices_permission = false;
  bool need_location_services = false;
  bool can_request_system_permissions = false;

#if BUILDFLAG(IS_ANDROID)
  WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host_.get());
  need_location_permission =
      permissions::NeedsLocationPermissionForBluetooth(web_contents);
  need_nearby_devices_permission =
      permissions::NeedsNearbyDevicesPermissionForBluetooth(web_contents);
  need_location_services = permissions::NeedsLocationServicesForBluetooth();
  can_request_system_permissions =
      permissions::CanRequestSystemPermissionsForBluetooth(web_contents);
#endif  // BUILDFLAG(IS_ANDROID)

  std::move(callback).Run(
      need_location_permission, need_nearby_devices_permission,
      need_location_services, can_request_system_permissions);
}

void BluetoothInternalsHandler::RequestSystemPermissions(
    RequestSystemPermissionsCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host_.get());
  permissions::RequestSystemPermissionsForBluetooth(web_contents);
#endif  // BUILDFLAG(IS_ANDROID)
  std::move(callback).Run();
}

void BluetoothInternalsHandler::RequestLocationServices(
    RequestLocationServicesCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host_.get());
  permissions::RequestLocationServices(web_contents);
#endif  // BUILDFLAG(IS_ANDROID)
  std::move(callback).Run();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void BluetoothInternalsHandler::RestartSystemBluetooth(
    RestartSystemBluetoothCallback callback) {
  if (bluetooth_system_state_ == BluetoothSystemState::kUnavailable) {
    std::move(callback).Run();
    return;
  }

  restart_system_bluetooth_callback_ = std::move(callback);
  bool enable = false;
  turning_bluetooth_off_ = true;

  if (bluetooth_system_state_ == BluetoothSystemState::kDisabled ||
      bluetooth_system_state_ == BluetoothSystemState::kDisabling) {
    enable = true;
    turning_bluetooth_on_ = true;
    turning_bluetooth_off_ = false;
  }

  bluetooth_initial_state_ = !enable;
  remote_cros_bluetooth_config_->SetBluetoothEnabledState(enable);
}

void BluetoothInternalsHandler::OnPropertiesUpdated(
    ash::bluetooth_config::mojom::BluetoothSystemPropertiesPtr properties) {
  bluetooth_system_state_ = properties->system_state;
  if (bluetooth_system_state_ == BluetoothSystemState::kUnavailable) {
    return;
  }

  if (!turning_bluetooth_off_ && !turning_bluetooth_on_) {
    return;
  }

  if (bluetooth_system_state_ == BluetoothSystemState::kDisabling ||
      bluetooth_system_state_ == BluetoothSystemState::kEnabling) {
    return;
  }

  CHECK(restart_system_bluetooth_callback_);
  if (bluetooth_system_state_ == BluetoothSystemState::kDisabled &&
      turning_bluetooth_off_) {
    turning_bluetooth_off_ = false;
    turning_bluetooth_on_ = true;
    remote_cros_bluetooth_config_->SetBluetoothEnabledState(true);
  }

  if (bluetooth_system_state_ == BluetoothSystemState::kEnabled &&
      turning_bluetooth_on_) {
    turning_bluetooth_on_ = false;
    turning_bluetooth_off_ = false;
    std::move(restart_system_bluetooth_callback_).Run();
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
