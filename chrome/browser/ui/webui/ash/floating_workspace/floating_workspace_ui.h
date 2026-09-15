// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UI_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/mojo_web_ui_controller.h"

class PrefService;

namespace ash {
class FloatingWorkspaceUI;
class FloatingWorkspaceDialogHandler;
// The WebUIConfig for the FloatingWorkspaceUI class.
class FloatingWorkspaceUIConfig : public content::WebUIConfig {
 public:
  // `local_state` must be non-null and must outlive `this`.
  explicit FloatingWorkspaceUIConfig(PrefService* local_state);
  FloatingWorkspaceUIConfig(const FloatingWorkspaceUIConfig&) = delete;
  FloatingWorkspaceUIConfig& operator=(const FloatingWorkspaceUIConfig&) =
      delete;
  ~FloatingWorkspaceUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  const raw_ref<PrefService> local_state_;
};

class FloatingWorkspaceUI : public ui::MojoWebDialogUI {
 public:
  FloatingWorkspaceUI(const PrefService& local_state, content::WebUI* web_ui);
  FloatingWorkspaceUI(const FloatingWorkspaceUI&) = delete;
  FloatingWorkspaceUI& operator=(const FloatingWorkspaceUI&) = delete;
  ~FloatingWorkspaceUI() override;

  // Instantiates implementation of the mojom::CrosNetworkConfig mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
          receiver);
  FloatingWorkspaceDialogHandler* GetMainHandler();

 private:
  raw_ptr<FloatingWorkspaceDialogHandler> main_handler_;
  base::WeakPtrFactory<FloatingWorkspaceUI> weak_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_FLOATING_WORKSPACE_FLOATING_WORKSPACE_UI_H_
