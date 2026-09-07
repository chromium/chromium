// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_UI_H_

#include <memory>

#include "ash/constants/webui_url_constants.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom-forward.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"

class ApplicationLocaleStorage;

namespace ash {

class ParentAccessUI;
class ParentAccessUiHandler;

// WebUIConfig for chrome://parent-access
class ParentAccessUIConfig : public content::WebUIConfig {
 public:
  // `application_locale_storage` must not be null and must outlive `this`.
  explicit ParentAccessUIConfig(
      const ApplicationLocaleStorage* application_locale_storage);
  ParentAccessUIConfig(const ParentAccessUIConfig&) = delete;
  ParentAccessUIConfig& operator=(const ParentAccessUIConfig&) = delete;
  ~ParentAccessUIConfig() override;

  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
};

// Controller for the ParentAccessUI, a WebUI which enables parent verification.
// It is hosted at chrome://parent-access.
class ParentAccessUI : public ui::MojoWebDialogUI {
 public:
  // `application_locale_storage` must not be null and must outlive `this`.
  ParentAccessUI(content::WebUI* web_ui,
                 const ApplicationLocaleStorage* application_locale_storage);
  ParentAccessUI(const ParentAccessUI&) = delete;
  ParentAccessUI& operator=(const ParentAccessUI&) = delete;

  ~ParentAccessUI() override;

  static void SetUpForTest(signin::IdentityManager* identity_manager);

  // Instantiates the implementor of the mojom::ParentAccessUiHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<parent_access_ui::mojom::ParentAccessUiHandler>
          receiver);

  parent_access_ui::mojom::ParentAccessUiHandler* GetHandlerForTest();

 private:
  void SetUpResources();

  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;

  std::unique_ptr<parent_access_ui::mojom::ParentAccessUiHandler>
      mojo_api_handler_;

  static signin::IdentityManager* test_identity_manager_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_UI_H_
