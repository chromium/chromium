// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PASSWORD_MANAGER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PASSWORD_MANAGER_UI_H_

#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"

namespace base {
class RefCountedMemory;
}

namespace extensions {
class PasswordsPrivateDelegate;
}

class PasswordManagerUI;

class PasswordManagerUIConfig
    : public content::DefaultWebUIConfig<PasswordManagerUI> {
 public:
  PasswordManagerUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           password_manager::kChromeUIPasswordManagerHost) {}

  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

class PasswordManagerUI : public ui::MojoWebUIController,
                          public help_bubble::mojom::HelpBubbleHandlerFactory {
 public:
  explicit PasswordManagerUI(content::WebUI* web_ui);
  ~PasswordManagerUI() override;

  PasswordManagerUI(const PasswordManagerUI&) = delete;
  PasswordManagerUI& operator=(const PasswordManagerUI&) = delete;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSettingsMenuItemElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAddShortcutElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOverflowMenuElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSharePasswordElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAccountStoreToggleElementId);
  DECLARE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(kAddShortcutCustomEventId);

  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          pending_receiver);

 private:
  scoped_refptr<extensions::PasswordsPrivateDelegate>
      passwords_private_delegate_;

  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_handler_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PASSWORD_MANAGER_UI_H_
