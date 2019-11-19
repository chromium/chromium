// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/discover/modules/discover_module_launch_help_app.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "content/public/browser/web_ui.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {

namespace {

class DiscoverModuleLaunchHelpAppHandler : public DiscoverHandler {
 public:
  explicit DiscoverModuleLaunchHelpAppHandler(
      JSCallsContainer* js_calls_container);
  ~DiscoverModuleLaunchHelpAppHandler() override = default;

 private:
  // BaseWebUIHandler: implementation
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;
  void RegisterMessages() override;

  // Message handlers.
  void HandleLaunchHelpApp();

  DISALLOW_COPY_AND_ASSIGN(DiscoverModuleLaunchHelpAppHandler);
};

DiscoverModuleLaunchHelpAppHandler::DiscoverModuleLaunchHelpAppHandler(
    JSCallsContainer* js_calls_container)
    : DiscoverHandler(js_calls_container) {}

void DiscoverModuleLaunchHelpAppHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("discoverGetHelp", IDS_DISCOVER_GET_HELP);
}

void DiscoverModuleLaunchHelpAppHandler::Initialize() {}

void DiscoverModuleLaunchHelpAppHandler::RegisterMessages() {
  AddCallback("discover.launch-help-app.handleLaunchHelpApp",
              &DiscoverModuleLaunchHelpAppHandler::HandleLaunchHelpApp);
}

void DiscoverModuleLaunchHelpAppHandler::HandleLaunchHelpApp() {
  chrome::ShowHelpForProfile(Profile::FromWebUI(web_ui()),
                             chrome::HelpSource::HELP_SOURCE_WEBUI);
}

}  // anonymous namespace

/* ***************************************************************** */
/* Discover LaunchHelpApp module implementation below.               */

const char DiscoverModuleLaunchHelpApp::kModuleName[] = "launch-help-app";

DiscoverModuleLaunchHelpApp::DiscoverModuleLaunchHelpApp() = default;

DiscoverModuleLaunchHelpApp::~DiscoverModuleLaunchHelpApp() = default;

bool DiscoverModuleLaunchHelpApp::IsCompleted() const {
  return false;
}

std::unique_ptr<DiscoverHandler>
DiscoverModuleLaunchHelpApp::CreateWebUIHandler(
    JSCallsContainer* js_calls_container) {
  return std::make_unique<DiscoverModuleLaunchHelpAppHandler>(
      js_calls_container);
}

}  // namespace chromeos
