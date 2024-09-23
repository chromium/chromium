// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/multidevice_internals/multidevice_internals_ui.h"

#include "ash/constants/ash_features.h"
#include "base/containers/span.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/multidevice_internals/multidevice_internals_logs_handler.h"
#include "chrome/browser/ui/webui/ash/multidevice_internals/multidevice_internals_phone_hub_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/multidevice_internals_resources.h"
#include "chrome/grit/multidevice_internals_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash {

MultideviceInternalsUI::MultideviceInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          Profile::FromWebUI(web_ui),
          chrome::kChromeUIMultiDeviceInternalsHost);
  html_source->AddBoolean("isPhoneHubEnabled", features::IsPhoneHubEnabled());

  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kMultideviceInternalsResources,
                      kMultideviceInternalsResourcesSize),
      IDR_MULTIDEVICE_INTERNALS_INDEX_HTML);

  web_ui->AddMessageHandler(
      std::make_unique<multidevice::MultideviceLogsHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<multidevice::MultidevicePhoneHubHandler>());
}

MultideviceInternalsUI::~MultideviceInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(MultideviceInternalsUI)

}  // namespace ash
