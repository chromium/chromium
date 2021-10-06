// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.h"

#include <utility>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {

namespace {

const char kParentAccessDefaultURL[] =
    "https://families.google.com/parentaccess#pac";
const char kParentAccessSwitch[] = "parent-access-url";

// Returns the URL of the Parent Access flow from the command-line switch,
// or the default value if it's not defined.
GURL GetParentAccessURL() {
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
  // TODO(b/200853161): Set caller id from params.
  std::string query_string = base::StringPrintf(
      "callerid=2fdd8d6e&cros-origin=chrome://parent-access");
  replacements.SetQueryStr(query_string);
  const GURL result = base_url.ReplaceComponents(replacements);
  DCHECK(result.is_valid()) << "Invalid URL \"" << url << "\" for switch \""
                            << kParentAccessSwitch << "\"";
  return result;
}

}  // namespace

ParentAccessUI::ParentAccessUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Set up the basic page framework.
  SetUpResources();
}

ParentAccessUI::~ParentAccessUI() = default;

void ParentAccessUI::BindInterface(
    mojo::PendingReceiver<parent_access_ui::mojom::ParentAccessUIHandler>
        receiver) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()));

  mojo_api_handler_ = std::make_unique<ParentAccessUIHandlerImpl>(
      std::move(receiver), web_ui(), identity_manager);
}

void ParentAccessUI::SetUpResources() {
  Profile* profile = Profile::FromWebUI(web_ui());
  std::unique_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUIParentAccessHost));

  // Initialize parent access URL from the command-line arguments (if provided).
  web_content_url_ = GetParentAccessURL();
  // The Polymer JS bundle requires this at the moment because it sets innerHTML
  // on an element, which violates the Trusted Types CSP.
  source->DisableTrustedTypesCSP();
  source->EnableReplaceI18nInJS();

  // Forward data to the WebUI.
  source->AddResourcePath("parent_access_ui.js", IDR_PARENT_ACCESS_UI_JS);

  source->AddLocalizedString("pageTitle", IDS_PARENT_ACCESS_PAGE_TITLE);
  source->AddResourcePath("parent_access_ui.mojom-lite.js",
                          IDR_PARENT_ACCESS_UI_MOJOM_LITE_JS);

  source->UseStringsJs();
  source->SetDefaultResource(IDR_PARENT_ACCESS_HTML);
  source->AddString("webviewUrl", web_content_url_.spec());
  source->AddString("eventOriginFilter", web_content_url_.GetOrigin().spec());
  source->AddString("platformVersion", base::SysInfo::OperatingSystemVersion());

  // Forward the browser language code.
  source->AddString(
      "languageCode",
      google_util::GetGoogleLocale(g_browser_process->GetApplicationLocale()));

  content::WebUIDataSource::Add(profile, source.release());
}

WEB_UI_CONTROLLER_TYPE_IMPL(ParentAccessUI)

}  // namespace chromeos
