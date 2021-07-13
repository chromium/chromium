// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/projector/projector_ui.h"

#include "ash/public/cpp/projector/projector_controller.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/projector/grit/projector_resources.h"
#include "chromeos/projector/grit/projector_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace chromeos {

namespace {

content::WebUIDataSource* CreateProjectorHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIProjectorHost);

  webui::SetupWebUIDataSource(
      source, base::make_span(kProjectorResources, kProjectorResourcesSize),
      IDR_PROJECTOR_PLAYER_APP_HTML);
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
        "launchScreenCapture",
        base::BindRepeating(
            &ProjectorMessageHandler::HandleShowHideProjectorToolbar,
            base::Unretained(this)));
  }

 private:
  void HandleShowHideProjectorToolbar(const base::ListValue* args) {
    AllowJavascript();

    CHECK_EQ(1u, args->GetSize());

    bool is_visible = true;
    // This code only shows and hides the Projector toolbar.
    // TODO(crbug/1206720): Integrate with screen capture.
    auto* projector_controller = ash::ProjectorController::Get();
    if (projector_controller && projector_controller->IsEligible()) {
      is_visible = projector_controller->AreProjectorToolsVisible();
      projector_controller->SetProjectorToolsVisible(!is_visible);
    }
    ResolveJavascriptCallback(args->GetList()[0], base::Value(!is_visible));
  }
};

}  // namespace

ProjectorUI::ProjectorUI(content::WebUI* web_ui)
    : MojoBubbleWebUIController(web_ui, /*enable_chrome_send=*/true) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateProjectorHTMLSource());

  web_ui->AddMessageHandler(std::make_unique<ProjectorMessageHandler>());
}

ProjectorUI::~ProjectorUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ProjectorUI)

}  // namespace chromeos
