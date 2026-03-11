// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_H_

#include "chrome/browser/ui/webui/policy/policy_ui_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "components/policy/resources/webui/mojom/policy.mojom-forward.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class Profile;

namespace content {
class WebUI;
}

class PolicyUI;

class PolicyUIConfig : public content::DefaultWebUIConfig<PolicyUI> {
 public:
  PolicyUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIPolicyHost) {}
};

// The Web UI controller for the chrome://policy page.
class PolicyUI : public ui::MojoWebUIController,
                 public policy::mojom::PolicyPageHandlerFactory {
 public:
  explicit PolicyUI(content::WebUI* web_ui);

  PolicyUI(const PolicyUI&) = delete;
  PolicyUI& operator=(const PolicyUI&) = delete;

  ~PolicyUI() override;

  void BindInterface(
      mojo::PendingReceiver<policy::mojom::PolicyPageHandlerFactory> receiver);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static bool ShouldLoadTestPage(Profile* profile);
  static base::Value GetSchema(Profile* profile);

 private:
  void CreateHandler(
      mojo::PendingReceiver<policy::mojom::PolicyPageHandler> handler,
      mojo::PendingRemote<policy::mojom::PolicyPageClient> client) override;

  std::unique_ptr<PolicyUIHandler> page_handler_;
  mojo::Receiver<policy::mojom::PolicyPageHandlerFactory> factory_receiver_{
      this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_H_
