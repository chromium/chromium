// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/sys_internals/sys_internals_ui.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/sys_internals/sys_internals_message_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

SysInternalsUI::SysInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<SysInternalsMessageHandler>());

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUISysInternalsHost);
  html_source->SetDefaultResource(IDR_SYS_INTERNALS_HTML);
  html_source->AddResourcePath("index.css", IDR_SYS_INTERNALS_CSS);
  html_source->AddResourcePath("index.js", IDR_SYS_INTERNALS_JS);
  html_source->AddResourcePath("constants.js", IDR_SYS_INTERNALS_CONSTANT_JS);
  html_source->AddResourcePath("line_chart.css",
                               IDR_SYS_INTERNALS_LINE_CHART_CSS);
  html_source->AddResourcePath("line_chart.js",
                               IDR_SYS_INTERNALS_LINE_CHART_JS);

  html_source->AddResourcePath("images/menu.svg",
                               IDR_SYS_INTERNALS_IMAGE_MENU_SVG);
  html_source->AddResourcePath("images/info.svg",
                               IDR_SYS_INTERNALS_IMAGE_INFO_SVG);
  html_source->AddResourcePath("images/cpu.svg",
                               IDR_SYS_INTERNALS_IMAGE_CPU_SVG);
  html_source->AddResourcePath("images/memory.svg",
                               IDR_SYS_INTERNALS_IMAGE_MEMORY_SVG);
  html_source->AddResourcePath("images/zram.svg",
                               IDR_SYS_INTERNALS_IMAGE_ZRAM_SVG);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
}

SysInternalsUI::~SysInternalsUI() {}
