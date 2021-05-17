// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ECHE_APP_UI_SYSTEM_INFO_PROVIDER_H_
#define CHROMEOS_COMPONENTS_ECHE_APP_UI_SYSTEM_INFO_PROVIDER_H_

#include "ash/public/cpp/tablet_mode_observer.h"
#include "chromeos/components/eche_app_ui/mojom/eche_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace eche_app {

extern const char kJsonDeviceNameKey[];
extern const char kJsonBoardNameKey[];

class SystemInfo;

// Provides the system information likes board/device names for EcheApp and
// exposes the interface via mojoa.
class SystemInfoProvider : public mojom::SystemInfoProvider,
                           public ash::ScreenBacklightObserver,
                           public ash::TabletModeObserver {
 public:
  explicit SystemInfoProvider(std::unique_ptr<SystemInfo> system_info);
  ~SystemInfoProvider() override;

  SystemInfoProvider(const SystemInfoProvider&) = delete;
  SystemInfoProvider& operator=(const SystemInfoProvider&) = delete;

  // mojom::SystemInfoProvider:
  void GetSystemInfo(
      base::OnceCallback<void(const std::string&)> callback) override;
  void SetSystemInfoObserver(
      mojo::PendingRemote<mojom::SystemInfoObserver> observer) override;

  void Bind(mojo::PendingReceiver<mojom::SystemInfoProvider> receiver);

 private:
  // ash::ScreenBacklightObserver overrides;
  void OnScreenBacklightStateChanged(
      ash::ScreenBacklightState screen_state) override;
  // ash:TabletModeObserver overrides.
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  void SetTabletModeChanged(bool enabled);
  mojo::Receiver<mojom::SystemInfoProvider> info_receiver_{this};
  mojo::Remote<mojom::SystemInfoObserver> observer_remote_;
  std::unique_ptr<SystemInfo> system_info_;
};

}  // namespace eche_app
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ECHE_APP_UI_SYSTEM_INFO_PROVIDER_H_
