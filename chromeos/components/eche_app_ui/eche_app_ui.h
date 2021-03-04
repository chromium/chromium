// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chromeos/components/eche_app_ui/mojom/eche_app.mojom-forward.h"
#include "ui/webui/mojo_web_ui_controller.h"

#ifndef CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_APP_UI_H_
#define CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_APP_UI_H_

namespace chromeos {
namespace eche_app {

// The WebUI for chrome://eche-app/.
class EcheAppUI : public ui::MojoWebUIController {
 public:
  using BindSignalingMessageExchangerCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::SignalingMessageExchanger>)>;

  EcheAppUI(content::WebUI* web_ui,
            BindSignalingMessageExchangerCallback exchanger_callback);
  EcheAppUI(const EcheAppUI&) = delete;
  EcheAppUI& operator=(const EcheAppUI&) = delete;
  ~EcheAppUI() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::SignalingMessageExchanger> receiver);

 private:
  const BindSignalingMessageExchangerCallback bind_exchanger_callback_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace eche_app
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_APP_UI_H_
