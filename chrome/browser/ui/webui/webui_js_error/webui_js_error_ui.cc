// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/webui_js_error/webui_js_error_ui.h"

#include <ios>

#include "base/feature_list.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/webui_js_error_resources.h"
#include "chrome/grit/webui_js_error_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"

WebUIJsErrorUI::WebUIJsErrorUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  VLOG(3) << "chrome://webuijserror loading.";

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIWebUIJsErrorHost);

  // As this is just a debugging page, we don't waste the translators' time by
  // actually translating the i18n strings. However, we still want to be able to
  // use this page to test the $i18n{} replacement system, so we still add
  // "translation" strings.
  source->AddString("title", "WebUI JavaScript Error Page");
  source->AddString("explainerText",
                    "This page generates a JavaScript error on load. Other\n"
                    "types of errors can be generated with the buttons:");
  source->AddString("logErrorButton", "Log Error");
  source->AddString("uncaughtErrorButton", "Throw Uncaught Error");
  source->AddString("promiseRejectButton", "Unhandled Promise Rejection");

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kWebuiJsErrorResources, kWebuiJsErrorResourcesSize),
      IDR_WEBUI_JS_ERROR_WEBUI_JS_ERROR_HTML);
}

WebUIJsErrorUI::~WebUIJsErrorUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(WebUIJsErrorUI)
