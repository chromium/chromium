// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/projector_app/trusted_projector_ui.h"

#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "base/values.h"
#include "chromeos/components/projector_app/projector_app_constants.h"
#include "chromeos/grit/chromeos_projector_app_trusted_resources.h"
#include "chromeos/grit/chromeos_projector_app_trusted_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace chromeos {

namespace {

content::WebUIDataSource* CreateProjectorHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chromeos::kChromeUIProjectorAppHost);

  source->AddResourcePaths(
      base::make_span(kChromeosProjectorAppTrustedResources,
                      kChromeosProjectorAppTrustedResourcesSize));

  std::string csp =
      std::string("frame-src ") + kChromeUIUntrustedProjectorAppUrl + ";";

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, csp);

  return source;
}

class ProjectorMessageHandler : public content::WebUIMessageHandler {
 public:
  ProjectorMessageHandler() = default;
  ~ProjectorMessageHandler() override = default;
  ProjectorMessageHandler(const ProjectorMessageHandler&) = delete;
  ProjectorMessageHandler& operator=(const ProjectorMessageHandler&) = delete;

  // content::WebUIMessageHandler:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "launchProjectorRecording",
        base::BindRepeating(
            &ProjectorMessageHandler::HandleShowHideProjectorToolbar,
            base::Unretained(this)));
  }

 private:
  void HandleShowHideProjectorToolbar(const base::ListValue* args) {
    AllowJavascript();

    CHECK_EQ(1u, args->GetSize());

    // This code only shows and hides the Projector toolbar.
    // TODO(crbug/1206720): Integrate with screen capture.
    auto* projector_controller = ash::ProjectorController::Get();
    if (!projector_controller || !projector_controller->IsEligible()) {
      ResolveJavascriptCallback(args->GetList()[0],
                                /*is_visible=*/base::Value(false));
      return;
    }
    auto* projector_session = ash::ProjectorSession::Get();
    DCHECK(projector_session);
    bool should_show = false;
    if (projector_session->is_active()) {
      projector_controller->SetProjectorToolsVisible(false);
      projector_session->Stop();
    } else {
      projector_session->Start(ash::SourceType::kUnset);
      projector_controller->SetProjectorToolsVisible(true);
      should_show = true;
    }
    ResolveJavascriptCallback(args->GetList()[0], base::Value(should_show));
  }
};

}  // namespace

TrustedProjectorUI::TrustedProjectorUI(content::WebUI* web_ui)
    : MojoBubbleWebUIController(web_ui, /*enable_chrome_send=*/true) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, CreateProjectorHTMLSource());

  web_ui->AddMessageHandler(std::make_unique<ProjectorMessageHandler>());
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
}

TrustedProjectorUI::~TrustedProjectorUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(TrustedProjectorUI)

}  // namespace chromeos
