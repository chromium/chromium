// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.mojom-forward.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui_handler_impl.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "url/gurl.h"

namespace chromeos {

// Controller for the ParentAccessUI, a WebUI which enables parent verification.
// It is hosted at chrome://parent-access.
class ParentAccessUI : public ui::MojoWebUIController {
 public:
  explicit ParentAccessUI(content::WebUI* web_ui);
  ParentAccessUI(const ParentAccessUI&) = delete;
  ParentAccessUI& operator=(const ParentAccessUI&) = delete;

  ~ParentAccessUI() override;

  static void SetUpForTest(signin::IdentityManager* identity_manager);

  // Instantiates the implementor of the mojom::ParentAccessUIHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<parent_access_ui::mojom::ParentAccessUIHandler>
          receiver);

  const GURL GetWebContentURLForTesting();
  parent_access_ui::mojom::ParentAccessUIHandler* GetHandlerForTest();

 private:
  void SetUpResources();

  std::unique_ptr<parent_access_ui::mojom::ParentAccessUIHandler>
      mojo_api_handler_;

  // The URL for the remote web content embedded in the WebUI's webview (not to
  // be confused with the chrome:// URL for the WebUI itself).
  GURL web_content_url_;

  static signin::IdentityManager* test_identity_manager_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_PARENT_ACCESS_PARENT_ACCESS_UI_H_
