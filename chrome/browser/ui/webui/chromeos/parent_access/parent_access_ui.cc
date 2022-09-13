// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.mojom.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

const char kParentAccessDefaultURL[] =
    "https://families.google.com/parentaccess";
const char kParentAccessSwitch[] = "parent-access-url";

// Returns the URL of the Parent Access flow from the command-line switch,
// or the default value if it's not defined.
GURL GetParentAccessURL(std::string caller_id,
                        std::string platform_version,
                        std::string language_code) {
  std::string url;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kParentAccessSwitch)) {
    url = command_line->GetSwitchValueASCII(kParentAccessSwitch);
  } else {
    url = kParentAccessDefaultURL;
    DCHECK(GURL(url).DomainIs("google.com"));
  }
  const GURL base_url(url);
  GURL::Replacements replacements;
  std::string query_string = base::StringPrintf(
      "callerid=%s&hl=%s&platform_version=%s&cros-origin=chrome://"
      "parent-access",
      caller_id.c_str(), language_code.c_str(), platform_version.c_str());
  replacements.SetQueryStr(query_string);
  const GURL result = base_url.ReplaceComponents(replacements);
  DCHECK(result.is_valid()) << "Invalid URL \"" << url << "\" for switch \""
                            << kParentAccessSwitch << "\"";
  return result;
}

}  // namespace

// static
signin::IdentityManager* ParentAccessUI::test_identity_manager_ = nullptr;

ParentAccessUI::ParentAccessUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Set up the basic page framework.
  SetUpResources();
}

ParentAccessUI::~ParentAccessUI() = default;

// static
void ParentAccessUI::SetUpForTest(signin::IdentityManager* identity_manager) {
  test_identity_manager_ = identity_manager;
}

void ParentAccessUI::BindInterface(
    mojo::PendingReceiver<parent_access_ui::mojom::ParentAccessUIHandler>
        receiver) {
  signin::IdentityManager* identity_manager =
      test_identity_manager_
          ? test_identity_manager_
          : IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()));

  mojo_api_handler_ = std::make_unique<ParentAccessUIHandlerImpl>(
      std::move(receiver), web_ui(), identity_manager);
}

const GURL ParentAccessUI::GetWebContentURLForTesting() {
  return web_content_url_;
}

parent_access_ui::mojom::ParentAccessUIHandler*
ParentAccessUI::GetHandlerForTest() {
  return mojo_api_handler_.get();
}

void ParentAccessUI::SetUpResources() {
  Profile* profile = Profile::FromWebUI(web_ui());
  std::unique_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUIParentAccessHost));

  web_content_url_ = GetParentAccessURL(
      "39454505", /* TODO(b/200853161): Set caller id from params. */
      base::SysInfo::OperatingSystemVersion(),
      google_util::GetGoogleLocale(g_browser_process->GetApplicationLocale()));

  // The Polymer JS bundle requires this at the moment because it sets innerHTML
  // on an element, which violates the Trusted Types CSP.
  source->DisableTrustedTypesCSP();
  source->EnableReplaceI18nInJS();

  // Forward data to the WebUI.
  source->AddResourcePath("parent_access_controller.js",
                          IDR_PARENT_ACCESS_CONTROLLER_JS);
  source->AddResourcePath("parent_access_app.js", IDR_PARENT_ACCESS_APP_JS);
  source->AddResourcePath("parent_access_ui.js", IDR_PARENT_ACCESS_UI_JS);
  source->AddResourcePath("parent_access_after.js", IDR_PARENT_ACCESS_AFTER_JS);
  source->AddResourcePath("flows/local_web_approvals_after.js",
                          IDR_LOCAL_WEB_APPROVALS_AFTER_JS);
  source->AddResourcePath("parent_access_ui.mojom-webui.js",
                          IDR_PARENT_ACCESS_UI_MOJOM_WEBUI_JS);
  source->AddResourcePath("images/parent_access_illustration_light_theme.svg",
                          IDR_PARENT_ACCESS_ILLUSTRATION_LIGHT_THEME_SVG);
  source->AddResourcePath("images/parent_access_illustration_dark_theme.svg",
                          IDR_PARENT_ACCESS_ILLUSTRATION_DARK_THEME_SVG);

  source->UseStringsJs();
  source->SetDefaultResource(IDR_PARENT_ACCESS_HTML);
  source->AddString("webviewUrl", web_content_url_.spec());
  // Set the filter to accept postMessages from the webviewURL's origin only.
  source->AddString("eventOriginFilter",
                    web_content_url_.GetWithEmptyPath().spec());

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"pageTitle", IDS_PARENT_ACCESS_PAGE_TITLE},
      {"approveButtonText", IDS_PARENT_ACCESS_AFTER_APPROVE_BUTTON},
      {"denyButtonText", IDS_PARENT_ACCESS_AFTER_DENY_BUTTON},
      {"localWebApprovalsAfterTitle",
       IDS_PARENT_ACCESS_LOCAL_WEB_APPROVALS_AFTER_TITLE},
      {"localWebApprovalsAfterSubtitle",
       IDS_PARENT_ACCESS_LOCAL_WEB_APPROVALS_AFTER_SUBTITLE},
      {"localWebApprovalsAfterDetails",
       IDS_PARENT_ACCESS_LOCAL_WEB_APPROVALS_AFTER_DETAILS},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  // Enables use of test_loader.html
  webui::SetJSModuleDefaults(source.get());

  // Allows loading of local content into an iframe for testing.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, "frame-src chrome://test/;");

  content::WebUIDataSource::Add(profile, source.release());
}

WEB_UI_CONTROLLER_TYPE_IMPL(ParentAccessUI)

}  // namespace chromeos
