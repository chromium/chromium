// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_js_exception/webui_js_exception_ui.h"

#include <ios>

#include "base/feature_list.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/webui_js_exception_resources.h"
#include "chrome/grit/webui_js_exception_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"

WebUIJsExceptionUI::WebUIJsExceptionUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
#if !defined(OS_WIN) && !defined(OS_FUCHSIA)
  VLOG(3) << std::boolalpha << "chrome://webuijsexception loading. "
          << "Experiment state: send javascript errors is "
          << base::FeatureList::IsEnabled(
                 features::kSendWebUIJavaScriptErrorReports)
          << " and send to prod is "
          << features::kWebUIJavaScriptErrorReportsSendToProductionParam.Get();
#else
  VLOG(3) << std::boolalpha << "chrome://webuijsexception loading.";
#endif

  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIWebUIJsExceptionHost);
  webui::SetupWebUIDataSource(source,
                              base::make_span(kWebuiJsExceptionResources,
                                              kWebuiJsExceptionResourcesSize),
                              std::string(),
                              IDR_WEBUI_JS_EXCEPTION_WEBUI_JS_EXCEPTION_HTML);
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, source);
}

WebUIJsExceptionUI::~WebUIJsExceptionUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(WebUIJsExceptionUI)
