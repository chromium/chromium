// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internals/internals_ui.h"

#include <vector>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/webui/internals/sessions/session_service_internals_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/webui/webui_util.h"

InternalsUIConfig::InternalsUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIInternalsHost) {}

namespace {

bool ShouldHandleWebUIRequestCallback(const std::string& path) {
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  if (SessionServiceInternalsHandler::ShouldHandleWebUIRequestCallback(path)) {
    return true;
  }
#endif
  return false;
}

void HandleWebUIRequestCallback(
    Profile* profile,
    const std::string& path,
    content::WebUIDataSource::GotDataCallback callback) {
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  if (SessionServiceInternalsHandler::ShouldHandleWebUIRequestCallback(path)) {
    return SessionServiceInternalsHandler::HandleWebUIRequestCallback(
        profile, path, std::move(callback));
  }
#endif
  NOTREACHED();
}

}  // namespace

InternalsUI::InternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  auto* profile = Profile::FromWebUI(web_ui);
  auto* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIInternalsHost);

  // chrome://internals/session-service
  source->SetRequestFilter(
      base::BindRepeating(&ShouldHandleWebUIRequestCallback),
      base::BindRepeating(&HandleWebUIRequestCallback, profile));
}

InternalsUI::~InternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(InternalsUI)
