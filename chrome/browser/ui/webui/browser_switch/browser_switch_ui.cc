// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/browser_switch/browser_switch_ui.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_switcher/alternative_browser_driver.h"
#include "chrome/browser/browser_switcher/browser_switcher_service.h"
#include "chrome/browser/browser_switcher/browser_switcher_service_factory.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/grit/components_resources.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

void GotoNewTabPage(content::WebContents* web_contents) {
  GURL url(chrome::kChromeUINewTabURL);
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
  web_contents->OpenURL(params);
}

// Returns true if there's only 1 tab left open in this profile. Incognito
// window tabs count as the same profile.
bool IsLastTab(const Profile* profile) {
  profile = profile->GetOriginalProfile();
  int tab_count = 0;
  for (const Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->GetOriginalProfile() != profile)
      continue;
    tab_count += browser->tab_strip_model()->count();
    if (tab_count > 1)
      return false;
  }
  return true;
}

// Returns a dictionary like:
//
// {
//   "sitelist": ["example.com", ...],
//   "greylist": ["example.net", ...]
// }
std::unique_ptr<base::Value> RuleSetToDict(
    const browser_switcher::RuleSet& ruleset) {
  auto sitelist = std::make_unique<base::ListValue>();
  for (const std::string& rule : ruleset.sitelist)
    sitelist->Append(rule);

  auto greylist = std::make_unique<base::ListValue>();
  for (const std::string& rule : ruleset.greylist)
    greylist->Append(rule);

  auto dict = std::make_unique<base::DictionaryValue>();
  dict->Set("sitelist", std::move(sitelist));
  dict->Set("greylist", std::move(greylist));

  return dict;
}

browser_switcher::BrowserSwitcherService* GetBrowserSwitcherService(
    content::WebUI* web_ui) {
  return browser_switcher::BrowserSwitcherServiceFactory::GetForBrowserContext(
      web_ui->GetWebContents()->GetBrowserContext());
}

content::WebUIDataSource* CreateBrowserSwitchUIHTMLSource(
    content::WebUI* web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIBrowserSwitchHost);

  auto* service = GetBrowserSwitcherService(web_ui);
  source->AddInteger("launchDelay", service->prefs().GetDelay());

  std::string browser_name = service->driver()->GetBrowserName();
  source->AddString("browserName", browser_name);

  if (browser_name.empty()) {
    // Browser name could not be auto-detected. Say "alternative browser"
    // instead of naming the browser.
    source->AddLocalizedString(
        "countdownTitle",
        IDS_ABOUT_BROWSER_SWITCH_COUNTDOWN_TITLE_UNKNOWN_BROWSER);
    source->AddLocalizedString(
        "description", IDS_ABOUT_BROWSER_SWITCH_DESCRIPTION_UNKNOWN_BROWSER);
    source->AddLocalizedString(
        "errorTitle", IDS_ABOUT_BROWSER_SWITCH_ERROR_TITLE_UNKNOWN_BROWSER);
    source->AddLocalizedString(
        "genericError", IDS_ABOUT_BROWSER_SWITCH_GENERIC_ERROR_UNKNOWN_BROWSER);
    source->AddLocalizedString(
        "openingTitle", IDS_ABOUT_BROWSER_SWITCH_OPENING_TITLE_UNKNOWN_BROWSER);
  } else {
    // Browser name was auto-detected. Name it in the text.
    source->AddLocalizedString(
        "countdownTitle",
        IDS_ABOUT_BROWSER_SWITCH_COUNTDOWN_TITLE_KNOWN_BROWSER);
    source->AddLocalizedString(
        "description", IDS_ABOUT_BROWSER_SWITCH_DESCRIPTION_KNOWN_BROWSER);
    source->AddLocalizedString(
        "errorTitle", IDS_ABOUT_BROWSER_SWITCH_ERROR_TITLE_KNOWN_BROWSER);
    source->AddLocalizedString(
        "genericError", IDS_ABOUT_BROWSER_SWITCH_GENERIC_ERROR_KNOWN_BROWSER);
    source->AddLocalizedString(
        "openingTitle", IDS_ABOUT_BROWSER_SWITCH_OPENING_TITLE_KNOWN_BROWSER);
  }

  source->AddLocalizedString("protocolError",
                             IDS_ABOUT_BROWSER_SWITCH_PROTOCOL_ERROR);
  source->AddLocalizedString("title", IDS_ABOUT_BROWSER_SWITCH_TITLE);

  source->AddResourcePath("app.js", IDR_BROWSER_SWITCH_APP_JS);
  source->AddResourcePath("browser_switch.html", IDR_BROWSER_SWITCH_HTML);
  source->AddResourcePath("browser_switch_proxy.js",
                          IDR_BROWSER_SWITCH_PROXY_JS);
  source->SetDefaultResource(IDR_BROWSER_SWITCH_HTML);

  // Setup chrome://browser-switch/internals debug UI.
  source->AddResourcePath("internals/browser_switch_internals.js",
                          IDR_BROWSER_SWITCH_INTERNALS_JS);
  source->AddResourcePath("internals/browser_switch_internals.html",
                          IDR_BROWSER_SWITCH_INTERNALS_HTML);
  source->AddResourcePath("internals/", IDR_BROWSER_SWITCH_INTERNALS_HTML);
  source->AddResourcePath("internals", IDR_BROWSER_SWITCH_INTERNALS_HTML);

  source->UseStringsJs();

  return source;
}

}  // namespace

class BrowserSwitchHandler : public content::WebUIMessageHandler {
 public:
  BrowserSwitchHandler();
  ~BrowserSwitchHandler() override;

  // WebUIMessageHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void OnAllRulesetsParsed(browser_switcher::BrowserSwitcherService* service);

  void OnBrowserSwitcherPrefsChanged(
      browser_switcher::BrowserSwitcherPrefs* prefs,
      const std::vector<std::string>& changed_prefs);

  // For the internals page: tell JS to update all the page contents.
  void UpdateEverything();

  // Launches the given URL in the configured alternative browser. Acts as a
  // bridge for |AlternativeBrowserDriver::TryLaunch()|. Then, if that succeeds,
  // closes the current tab.
  //
  // If it fails, the JavaScript promise is rejected. If it succeeds, the
  // JavaScript promise is not resolved, because we close the tab anyways.
  void HandleLaunchAlternativeBrowserAndCloseTab(const base::ListValue* args);

  // Navigates to the New Tab Page.
  void HandleGotoNewTabPage(const base::ListValue* args);

  // Resolves a promise with a JSON object with all the LBS rulesets, formatted
  // like this:
  //
  // {
  //   "gpo": {
  //     "sitelist": ["example.com", ...],
  //     "greylist": [...]
  //   },
  //   "ieem": { "sitelist": [...], "greylist": [...] },
  //   "external": { "sitelist": [...], "greylist": [...] }
  // }
  void HandleGetAllRulesets(const base::ListValue* args);

  // Resolves a promise with a JSON object describing the decision for a URL
  // (stay/go) + reason. The result is formatted like this:
  //
  // {
  //   "action": ("stay"|"go"),
  //   "reason": ("globally_disabled"|"protocol"|"sitelist"|...),
  //   "matching_rule": (string|undefined)
  // }
  void HandleGetDecision(const base::ListValue* args);

  // Resolves a promise with the time of the last policy fetch and next policy
  // fetch, as JS timestamps.
  //
  // {
  //   "last_fetch": 123456789,
  //   "next_fetch": 234567890
  // }
  void HandleGetTimestamps(const base::ListValue* args);

  // Resolves a promise with the configured sitelist XML download URLs. The keys
  // are the name of the pref associated with the sitelist.
  //
  // {
  //   "browser_switcher": {
  //     "use_ie_sitelist": "http://example.com/sitelist.xml",
  //     "external_sitelist_url": "http://example.com/other_sitelist.xml",
  //     "external_greylist_url": null
  //   }
  // }
  void HandleGetRulesetSources(const base::ListValue* args);

  // Immediately re-download and apply XML rules.
  void HandleRefreshXml(const base::ListValue* args);

  std::unique_ptr<browser_switcher::BrowserSwitcherPrefs::CallbackSubscription>
      prefs_subscription_;

  std::unique_ptr<
      browser_switcher::BrowserSwitcherService::CallbackSubscription>
      service_subscription_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSwitchHandler);
};

BrowserSwitchHandler::BrowserSwitchHandler() {}

BrowserSwitchHandler::~BrowserSwitchHandler() = default;

void BrowserSwitchHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "launchAlternativeBrowserAndCloseTab",
      base::BindRepeating(
          &BrowserSwitchHandler::HandleLaunchAlternativeBrowserAndCloseTab,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "gotoNewTabPage",
      base::BindRepeating(&BrowserSwitchHandler::HandleGotoNewTabPage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getAllRulesets",
      base::BindRepeating(&BrowserSwitchHandler::HandleGetAllRulesets,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDecision",
      base::BindRepeating(&BrowserSwitchHandler::HandleGetDecision,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getTimestamps",
      base::BindRepeating(&BrowserSwitchHandler::HandleGetTimestamps,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getRulesetSources",
      base::BindRepeating(&BrowserSwitchHandler::HandleGetRulesetSources,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "refreshXml", base::BindRepeating(&BrowserSwitchHandler::HandleRefreshXml,
                                        base::Unretained(this)));
}

void BrowserSwitchHandler::OnJavascriptAllowed() {
  auto* service = GetBrowserSwitcherService(web_ui());
  prefs_subscription_ = service->prefs().RegisterPrefsChangedCallback(
      base::BindRepeating(&BrowserSwitchHandler::OnBrowserSwitcherPrefsChanged,
                          base::Unretained(this)));
  service_subscription_ =
      service->RegisterAllRulesetsParsedCallback(base::BindRepeating(
          &BrowserSwitchHandler::OnAllRulesetsParsed, base::Unretained(this)));
}

void BrowserSwitchHandler::OnJavascriptDisallowed() {
  prefs_subscription_.reset();
  service_subscription_.reset();
}

void BrowserSwitchHandler::OnAllRulesetsParsed(
    browser_switcher::BrowserSwitcherService* service) {
  UpdateEverything();
}

void BrowserSwitchHandler::OnBrowserSwitcherPrefsChanged(
    browser_switcher::BrowserSwitcherPrefs* prefs,
    const std::vector<std::string>& changed_prefs) {
  UpdateEverything();
}

void BrowserSwitchHandler::UpdateEverything() {
  CallJavascriptFunction("updateEverything", base::Value());
}

void BrowserSwitchHandler::HandleLaunchAlternativeBrowserAndCloseTab(
    const base::ListValue* args) {
  DCHECK(args);
  AllowJavascript();

  std::string callback_id = args->GetList()[0].GetString();
  std::string url_spec = args->GetList()[1].GetString();
  GURL url(url_spec);

  auto* service = GetBrowserSwitcherService(web_ui());
  bool should_switch = service->sitelist()->ShouldSwitch(url);
  if (!url.is_valid() || !should_switch) {
    // This URL shouldn't open in an alternative browser. Abort launch, because
    // something weird is going on (e.g. race condition from a new sitelist
    // being loaded).
    RejectJavascriptCallback(args->GetList()[0], base::Value());
    return;
  }

  bool success;
  {
    SCOPED_UMA_HISTOGRAM_TIMER("BrowserSwitcher.LaunchTime");
    success = service->driver()->TryLaunch(url);
    UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.LaunchSuccess", success);
  }

  if (!success) {
    RejectJavascriptCallback(args->GetList()[0], base::Value());
    return;
  }

  auto* profile = Profile::FromWebUI(web_ui());

  if (service->prefs().KeepLastTab() && IsLastTab(profile)) {
    GotoNewTabPage(web_ui()->GetWebContents());
  } else {
    // We don't need to resolve the promise, because the tab will close anyways.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&content::WebContents::ClosePage,
                       base::Unretained(web_ui()->GetWebContents())));
  }
}

void BrowserSwitchHandler::HandleGotoNewTabPage(const base::ListValue* args) {
  GotoNewTabPage(web_ui()->GetWebContents());
}

void BrowserSwitchHandler::HandleGetAllRulesets(const base::ListValue* args) {
  DCHECK(args);
  AllowJavascript();

  auto* service = GetBrowserSwitcherService(web_ui());

  base::DictionaryValue retval;
  auto gpo_dict = RuleSetToDict(service->prefs().GetRules());
  retval.Set("gpo", std::move(gpo_dict));
  auto ieem_dict = RuleSetToDict(*service->sitelist()->GetIeemSitelist());
  retval.Set("ieem", std::move(ieem_dict));
  auto external_dict =
      RuleSetToDict(*service->sitelist()->GetExternalSitelist());
  retval.Set("external", std::move(external_dict));

  ResolveJavascriptCallback(args->GetList()[0], retval);
}

void BrowserSwitchHandler::HandleGetDecision(const base::ListValue* args) {
  DCHECK(args);
  AllowJavascript();

  GURL url = GURL(args->GetList()[1].GetString());
  if (!url.is_valid()) {
    RejectJavascriptCallback(args->GetList()[0], base::Value());
    return;
  }

  auto* service = GetBrowserSwitcherService(web_ui());
  browser_switcher::Decision decision = service->sitelist()->GetDecision(url);

  base::DictionaryValue retval;

  base::StringPiece action_name =
      (decision.action == browser_switcher::kStay) ? "stay" : "go";
  retval.Set("action", std::make_unique<base::Value>(action_name));

  base::StringPiece reason_name;
  switch (decision.reason) {
    case browser_switcher::kDisabled:
      reason_name = "globally_disabled";
      break;
    case browser_switcher::kProtocol:
      reason_name = "protocol";
      break;
    case browser_switcher::kSitelist:
      reason_name = "sitelist";
      break;
    case browser_switcher::kGreylist:
      reason_name = "greylist";
      break;
    case browser_switcher::kDefault:
      reason_name = "default";
      break;
  }
  retval.Set("reason", std::make_unique<base::Value>(reason_name));

  if (!decision.matching_rule.empty()) {
    retval.Set("matching_rule",
               std::make_unique<base::Value>(decision.matching_rule));
  }

  ResolveJavascriptCallback(args->GetList()[0], retval);
}

void BrowserSwitchHandler::HandleGetTimestamps(const base::ListValue* args) {
  DCHECK(args);
  AllowJavascript();

  auto* service = GetBrowserSwitcherService(web_ui());
  auto* downloader = service->sitelist_downloader();

  if (!downloader) {
    ResolveJavascriptCallback(args->GetList()[0], base::Value());
    return;
  }

  base::DictionaryValue retval;
  retval.Set("last_fetch", std::make_unique<base::Value>(
                               downloader->last_refresh_time().ToJsTime()));
  retval.Set("next_fetch", std::make_unique<base::Value>(
                               downloader->next_refresh_time().ToJsTime()));

  ResolveJavascriptCallback(args->GetList()[0], retval);
}

void BrowserSwitchHandler::HandleGetRulesetSources(
    const base::ListValue* args) {
  DCHECK(args);
  AllowJavascript();

  auto* service = GetBrowserSwitcherService(web_ui());
  std::vector<browser_switcher::RulesetSource> sources =
      service->GetRulesetSources();

  base::DictionaryValue retval;
  for (const auto& source : sources) {
    std::unique_ptr<base::Value> val;
    if (source.url.is_valid())
      val = std::make_unique<base::Value>(source.url.spec());
    else
      val = std::make_unique<base::Value>();
    // |pref_name| is something like "browser_switcher.blah", so this will be in
    // a nested object.
    retval.Set(source.pref_name, std::move(val));
  }
  ResolveJavascriptCallback(args->GetList()[0], retval);
}

void BrowserSwitchHandler::HandleRefreshXml(const base::ListValue* args) {
  DCHECK(args);
  auto* service = GetBrowserSwitcherService(web_ui());
  service->StartDownload(base::TimeDelta());
}

BrowserSwitchUI::BrowserSwitchUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<BrowserSwitchHandler>());

  // Set up the chrome://browser-switch source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile,
                                CreateBrowserSwitchUIHTMLSource(web_ui));
}
