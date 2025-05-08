// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/test/base/web_ui_test_data_source.h"

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/data/grit/webui_test_resources.h"
#include "chrome/test/data/grit/webui_test_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/resources/grit/webui_resources.h"
#include "ui/webui/webui_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/webui/common/trusted_types_util.h"
#endif

namespace {

void SetupTestDataSource(content::WebUIDataSource* source,
                         const std::string& scheme) {
#if BUILDFLAG(IS_CHROMEOS)
  ash::EnableTrustedTypesCSP(source);
  // Add lit-html-desktop policy so that Desktop UI components used by
  // cross-platform UIs (e.g. chrome://print, chrome://history) can be tested
  // on CrOS builds.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      base::StrCat({ash::kDefaultTrustedTypesPolicies, " lit-html-desktop;"}));
#else
  webui::EnableTrustedTypesCSP(source);
#endif

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      base::StringPrintf("script-src %s://* 'self';", scheme.c_str()));
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      base::StringPrintf("worker-src blob: %s://* 'self';", scheme.c_str()));
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameAncestors,
      base::StringPrintf("frame-ancestors %s://* 'self';", scheme.c_str()));
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, "frame-src 'self';");

  source->AddResourcePaths(kWebuiTestResources);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
}

}  // namespace

namespace webui {

content::WebUIDataSource* CreateAndAddWebUITestDataSource(
    content::BrowserContext* browser_context) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, chrome::kChromeUIWebUITestHost);

  SetupTestDataSource(source, content::kChromeUIScheme);
  return source;
}

content::WebUIDataSource* CreateAndAddUntrustedWebUITestDataSource(
    content::BrowserContext* browser_context) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, chrome::kChromeUIUntrustedWebUITestURL);

  SetupTestDataSource(source, content::kChromeUIUntrustedScheme);
  return source;
}

}  // namespace webui
