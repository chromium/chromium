// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_UI_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/app_management/app_management.mojom-forward.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom.h"
#include "chrome/browser/ui/webui/nearby_share/public/mojom/nearby_share_settings.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/app_management/app_management_page_handler_factory.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/user_action_recorder.mojom-forward.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "chromeos/services/cellular_setup/public/mojom/cellular_setup.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace chromeos {
namespace settings {

namespace mojom {
class SearchHandler;
}  // namespace mojom

// The WebUI handler for chrome://os-settings.
class OSSettingsUI : public ui::MojoWebUIController {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit OSSettingsUI(content::WebUI* web_ui);
  ~OSSettingsUI() override;

  // Instantiates implementor of the mojom::CellularSetup mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<cellular_setup::mojom::CellularSetup> receiver);

  // Instantiates implementor of the mojom::CrosNetworkConfig mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<network_config::mojom::CrosNetworkConfig> receiver);

  // Instantiates implementor of the mojom::UserActionRecorder mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<mojom::UserActionRecorder> receiver);

  // Instantiates implementor of the mojom::SearchHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<mojom::SearchHandler> receiver);

  // Instantiates implementor of the mojom::PageHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<app_management::mojom::PageHandlerFactory>
          receiver);

  // Binds to the existing settings instance owned by the nearby share keyed
  // service.
  void BindInterface(
      mojo::PendingReceiver<nearby_share::mojom::NearbyShareSettings> receiver);

  // Creates and binds a new receive manager.
  void BindInterface(
      mojo::PendingReceiver<nearby_share::mojom::ReceiveManager> receiver);

  // Binds to the existing contacts manager instance owned by the nearby share
  // keyed service.
  void BindInterface(
      mojo::PendingReceiver<nearby_share::mojom::ContactManager> receiver);

 private:
  base::TimeTicks time_when_opened_;

  WebuiLoadTimer webui_load_timer_;

  std::unique_ptr<mojom::UserActionRecorder> user_action_recorder_;
  std::unique_ptr<AppManagementPageHandlerFactory>
      app_management_page_handler_factory_;

  WEB_UI_CONTROLLER_TYPE_DECL();

  DISALLOW_COPY_AND_ASSIGN(OSSettingsUI);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_UI_H_
