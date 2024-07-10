// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals.mojom.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_handler.h"
#include "chrome/browser/ui/webui/privacy_sandbox/related_website_sets/related_website_sets.mojom.h"
#include "chrome/browser/ui/webui/privacy_sandbox/related_website_sets/related_website_sets_handler.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace privacy_sandbox_internals {

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

  void BindInterface(
      mojo::PendingReceiver<
          related_website_sets::mojom::RelatedWebsiteSetsPageHandler> receiver);

 private:
  std::unique_ptr<PrivacySandboxInternalsHandler> handler_;
  std::unique_ptr<RelatedWebsiteSetsHandler> related_website_sets_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace privacy_sandbox_internals

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_INTERNALS_UI_H_
