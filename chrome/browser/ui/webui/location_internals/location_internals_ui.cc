// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/location_internals/location_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/location_internals/location_internals_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/location_internals_resources.h"
#include "chrome/grit/location_internals_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/resources/grit/webui_resources.h"

LocationInternalsUI::LocationInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://location-internals source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUILocationInternalsHost);

  webui::SetupWebUIDataSource(source,
                              base::make_span(kLocationInternalsResources,
                                              kLocationInternalsResourcesSize),
                              IDR_LOCATION_INTERNALS_LOCATION_INTERNALS_HTML);
}

LocationInternalsUI::~LocationInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(LocationInternalsUI)

void LocationInternalsUI::BindInterface(
    mojo::PendingReceiver<mojom::LocationInternalsHandler> receiver) {
  handler_ = std::make_unique<LocationInternalsHandler>(std::move(receiver));
}
