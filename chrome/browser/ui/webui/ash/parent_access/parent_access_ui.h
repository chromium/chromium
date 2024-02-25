// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom-forward.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}

namespace ash {

class ParentAccessUI;
class ParentAccessUiHandler;

// WebUIConfig for chrome://parent-access
class ParentAccessUIConfig
    : public content::DefaultWebUIConfig<ParentAccessUI> {
 public:
  ParentAccessUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIParentAccessHost) {}
};

// Controller for the ParentAccessUI, a WebUI which enables parent verification.
// It is hosted at chrome://parent-access.
class ParentAccessUI : public ui::MojoWebDialogUI {
 public:
  explicit ParentAccessUI(content::WebUI* web_ui);
  ParentAccessUI(const ParentAccessUI&) = delete;
  ParentAccessUI& operator=(const ParentAccessUI&) = delete;

  ~ParentAccessUI() override;

  static void SetUpForTest(signin::IdentityManager* identity_manager);

  // Instantiates the implementor of the mojom::ParentAccessUiHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<parent_access_ui::mojom::ParentAccessUiHandler>
          receiver);

  // Instantiates the implementor of the mojom::PageHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  parent_access_ui::mojom::ParentAccessUiHandler* GetHandlerForTest();

 private:
  void SetUpResources();

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  std::unique_ptr<parent_access_ui::mojom::ParentAccessUiHandler>
      mojo_api_handler_;

  static signin::IdentityManager* test_identity_manager_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_UI_H_
