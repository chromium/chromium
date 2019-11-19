// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/discover/modules/discover_module_welcome.h"

#include "chrome/browser/ui/webui/chromeos/login/discover/discover_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {

namespace {

class DiscoverModuleWelcomeHandler : public DiscoverHandler {
 public:
  explicit DiscoverModuleWelcomeHandler(JSCallsContainer* js_calls_container);
  ~DiscoverModuleWelcomeHandler() override = default;

 private:
  // BaseWebUIHandler: implementation
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;
  void RegisterMessages() override;

  DISALLOW_COPY_AND_ASSIGN(DiscoverModuleWelcomeHandler);
};

DiscoverModuleWelcomeHandler::DiscoverModuleWelcomeHandler(
    JSCallsContainer* js_calls_container)
    : DiscoverHandler(js_calls_container) {}

void DiscoverModuleWelcomeHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->AddF("discoverWelcomeTitle", IDS_DISCOVER_WELCOME_TITLE,
                ui::GetChromeOSDeviceName());
  builder->AddF("discoverWelcomeSubTitle", IDS_DISCOVER_WELCOME_SUBTITLE,
                ui::GetChromeOSDeviceName());
}

void DiscoverModuleWelcomeHandler::Initialize() {}

void DiscoverModuleWelcomeHandler::RegisterMessages() {}

}  // anonymous namespace

/* ***************************************************************** */
/* Discover Welcome module implementation below.                     */

const char DiscoverModuleWelcome::kModuleName[] = "welcome";

DiscoverModuleWelcome::DiscoverModuleWelcome() = default;
DiscoverModuleWelcome::~DiscoverModuleWelcome() = default;

bool DiscoverModuleWelcome::IsCompleted() const {
  return false;
}

std::unique_ptr<DiscoverHandler> DiscoverModuleWelcome::CreateWebUIHandler(
    JSCallsContainer* js_calls_container) {
  return std::make_unique<DiscoverModuleWelcomeHandler>(js_calls_container);
}

}  // namespace chromeos
