// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_util.h"

#include "build/build_config.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/resources/grit/webui_resources_map.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#elif defined(OS_WIN) || defined(OS_MAC)
#include "base/enterprise_util.h"
#endif

namespace webui {

namespace {

void SetupPolymer3Defaults(content::WebUIDataSource* source) {
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");
  // TODO(crbug.com/1098690): Trusted Type Polymer
  source->DisableTrustedTypesCSP();
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER_HTML);
}

}  // namespace

void SetupWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const GritResourceMap> resources,
                          const std::string& generated_path,
                          int default_resource) {
  SetupPolymer3Defaults(source);
  // TODO (crbug.com/1132403): Replace usages of |generated_path| with the new
  // |resource_path| GRD property, and remove from here.
  bool has_gen_path = !generated_path.empty();
  for (const GritResourceMap& resource : resources) {
    std::string path = resource.name;
    if (has_gen_path && path.rfind(generated_path, 0) == 0) {
      path = path.substr(generated_path.size());
    }

    source->AddResourcePath(path, resource.value);
  }
  source->AddResourcePath("", default_resource);
}

void AddLocalizedStringsBulk(content::WebUIDataSource* html_source,
                             base::span<const LocalizedString> strings) {
  for (const auto& str : strings)
    html_source->AddLocalizedString(str.name, str.id);
}

void AddResourcePathsBulk(content::WebUIDataSource* source,
                          base::span<const ResourcePath> paths) {
  for (const auto& path : paths)
    source->AddResourcePath(path.path, path.id);
}

void AddResourcePathsBulk(content::WebUIDataSource* source,
                          base::span<const GritResourceMap> resources) {
  for (const auto& resource : resources)
    source->AddResourcePath(resource.name, resource.value);
}

bool IsEnterpriseManaged() {
#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->IsEnterpriseManaged();
#elif defined(OS_WIN) || defined(OS_MAC)
  return base::IsMachineExternallyManaged();
#else
  return false;
#endif
}

}  // namespace webui
