// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals.mojom.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_handler.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/privacy_sandbox/private_state_tokens/private_state_tokens.mojom.h"
#include "chrome/browser/ui/webui/privacy_sandbox/private_state_tokens/private_state_tokens_handler.h"
#include "chrome/browser/ui/webui/privacy_sandbox/related_website_sets/related_website_sets.mojom.h"
#include "chrome/browser/ui/webui/privacy_sandbox/related_website_sets/related_website_sets_handler.h"
#endif

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace privacy_sandbox_internals {

class PrivacySandboxInternalsUI;

class PrivacySandboxInternalsUIConfig
    : public content::DefaultWebUIConfig<PrivacySandboxInternalsUI> {
 public:
  PrivacySandboxInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIPrivacySandboxInternalsHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// MojoWebUIController for Privacy Sandbox Internals DevUI
class PrivacySandboxInternalsUI : public ui::MojoWebUIController {
 public:
  explicit PrivacySandboxInternalsUI(content::WebUI* web_ui);

  ~PrivacySandboxInternalsUI() override;

  PrivacySandboxInternalsUI(const PrivacySandboxInternalsUI&) = delete;
  PrivacySandboxInternalsUI& operator=(const PrivacySandboxInternalsUI&) =
      delete;

  void BindInterface(
      mojo::PendingReceiver<privacy_sandbox_internals::mojom::PageHandler>
          receiver);

#if !BUILDFLAG(IS_ANDROID)
  void BindInterface(
      mojo::PendingReceiver<
          related_website_sets::mojom::RelatedWebsiteSetsPageHandler> receiver);

  void BindInterface(
      mojo::PendingReceiver<
          private_state_tokens::mojom::PrivateStateTokensPageHandler> receiver);
#endif

 private:
  std::unique_ptr<PrivacySandboxInternalsHandler> handler_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<RelatedWebsiteSetsHandler> related_website_sets_handler_;
  std::unique_ptr<PrivateStateTokensHandler> private_state_tokens_handler_;
#endif

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace privacy_sandbox_internals

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_INTERNALS_UI_H_
