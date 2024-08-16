// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/browser_switch/browser_switch_ui.h"

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
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
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_switch_resources.h"
#include "chrome/grit/browser_switch_resources_map.h"
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
  web_contents->OpenURL(params, /*navigation_handle_callback=*/{});
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
base::Value::Dict RuleSetToDict(const browser_switcher::RuleSet& ruleset) {
  base::Value::List sitelist;
  for (const auto& rule : ruleset.sitelist) {
    sitelist.Append(rule->ToString());
  }

  base::Value::List greylist;
  for (const auto& rule : ruleset.greylist) {
    greylist.Append(rule->ToString());
  }

  return base::Value::Dict()
      .Set("sitelist", std::move(sitelist))
      .Set("greylist", std::move(greylist));
}

browser_switcher::BrowserSwitcherService* GetBrowserSwitcherService(
    content::WebUI* web_ui) {
  return browser_switcher::BrowserSwitcherServiceFactory::GetForBrowserContext(
      web_ui->GetWebContents()->GetBrowserContext());
}

void CreateAndAddBrowserSwitchUIHTMLSource(content::WebUI* web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIBrowserSwitchHost);

  auto* service = GetBrowserSwitcherService(web_ui);
  source->AddInteger("launchDelay", service->prefs().GetDelay());

  std::string alt_browser_name = service->driver()->GetBrowserName();
  source->AddString("altBrowserName", alt_browser_name);

  source->AddLocalizedString("browserName", IDS_PRODUCT_NAME);

  if (alt_browser_name.empty()) {
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

  static constexpr webui::LocalizedString kStrings[] = {
      {"switchInternalDescription", IDS_ABOUT_BROWSER_SWITCH_INTERNALS_DESC},
      {"switchInternalTitle", IDS_ABOUT_BROWSER_SWITCH_INTERNALS_TITLE},
      {"nothingShown", IDS_ABOUT_BROWSER_SWITCH_INTERNALS_NOTHING_SHOWN},
      {"switcherDisabled", IDS_ABOUT_BROWSER_SWITCH_INTERNALS_LBS_DISABLED},
      {"urlCheckerTitle", IDS_ABOUT_BROWSER_SWITCH_INTERNALS_URL_CHECKER_TITLE},
      {"urlCheckerDesc", IDS_ABOUT_BROWSER_SWITCH_INTERNALS_URL_CHECKER_DESC},
      {"openBrowser", IDS_ABOUT_BROWSER_SWITCH_INTERNALS_OPEN_BROWSER},
      {"openBrowserProtocolReason",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_OPEN_BROWSER_PROTOCOL_REASON},
      {"openBrowserDefaultReason",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_OPEN_BROWSER_DEFAULT_REASON},
      {"openBrowserRuleReason",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_OPEN_BROWSER_RULE_REASON},
      {"openBrowserInvertRuleReason",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_OPEN_BROWSER_INVERT_RULE_REASON},
      {"invalidURL", IDS_ABOUT_BROWSER_SWITCH_INTERNALS_INVALID_URL},
      {"xmlTitle", IDS_ABOUT_BROWSER_SWITCH_INTERNALS_XML_TITLE},
      {"xmlDesc", IDS_ABOUT_BROWSER_SWITCH_INTERNALS_XML_DESC},
      {"xmlSource", IDS_ABOUT_BROWSER_SWITCH_INTERNALS_XML_SOURCE},
      {"notConfigured", IDS_ABOUT_BROWSER_SWITCH_INTERNALS_XML_NOT_CONFIGURED},
      {"sitelistNotFetched",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_XML_SITELIST_NOT_FETCHED},
      {"sitelistDownloadButton",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_XML_SITELIST_DOWNLOAD_BUTTON},
      {"xmlSitelistLastDownloadDate",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_XML_SITELIST_LAST_DOWNLOAD_DATE},
      {"xmlSitelistNextDownloadDate",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_XML_SITELIST_NEXT_DOWNLOAD_DATE},
      {"forceOpenTitle",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_FORCE_OPEN_IN_TITLE},
      {"forceOpenDescription",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_FORCE_OPEN_IN_DESCRIPTION},
      {"forceOpenParagraph1",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_FORCE_OPEN_IN_FIRST_PARAGRAPH},
      {"forceOpenParagraph2",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_FORCE_OPEN_IN_SECOND_PARAGRAPH},
      {"forceOpenTableColumnRule",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_FORCE_OPEN_TABLE_COLUMN_RULE},
      {"forceOpenTableColumnOpensIn",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_FORCE_OPEN_TABLE_COLUMN_OPENS_IN},
      {"forceOpenTableColumnSource",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_FORCE_OPEN_TABLE_COLUMN_SOURCE},
      {"ignoreTitle", IDS_ABOUT_BROWSER_SWITCH_INTERNALS_IGNORE_TITLE},
      {"ignoreDescription",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_IGNORE_DESCRIPTION},
      {"ignoreParagraph1",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_IGNORE_FIRST_PARAGRAPH},
      {"ignoreParagraph2",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_IGNORE_SECOND_PARAGRAPH},
      {"ignoreTableColumnRule",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_IGNORE_TABLE_COLUMN_RULE},
      {"ignoreTableColumnSource",
       IDS_ABOUT_BROWSER_SWITCH_INTERNALS_IGNORE_TABLE_COLUMN_SOURCE},
  };
  source->AddLocalizedStrings(kStrings);

  source->AddLocalizedString("protocolError",
                             IDS_ABOUT_BROWSER_SWITCH_PROTOCOL_ERROR);
  source->AddLocalizedString("title", IDS_ABOUT_BROWSER_SWITCH_TITLE);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kBrowserSwitchResources, kBrowserSwitchResourcesSize),
      IDR_BROWSER_SWITCH_BROWSER_SWITCH_HTML);

  // Setup chrome://browser-switch/internals debug UI.
  source->AddResourcePath(
      "internals/", IDR_BROWSER_SWITCH_INTERNALS_BROWSER_SWITCH_INTERNALS_HTML);
  source->AddResourcePath(
      "internals", IDR_BROWSER_SWITCH_INTERNALS_BROWSER_SWITCH_INTERNALS_HTML);

  source->UseStringsJs();
}

}  // namespace

class BrowserSwitchHandler : public content::WebUIMessageHandler {
 public:
  BrowserSwitchHandler();

  BrowserSwitchHandler(const BrowserSwitchHandler&) = delete;
  BrowserSwitchHandler& operator=(const BrowserSwitchHandler&) = delete;

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
  void SendDataChangedEvent();

  // Launches the given URL in the configured alternative browser. Acts as a
  // bridge for |AlternativeBrowserDriver::TryLaunch()|. Then, if that succeeds,
  // closes the current tab.
  //
  // If it fails, the JavaScript promise is rejected. If it succeeds, the
  // JavaScript promise is not resolved, because we close the tab anyways.
  void HandleLaunchAlternativeBrowserAndCloseTab(const base::Value::List& args);

  void OnLaunchFinished(base::TimeTicks start,
                        std::string callback_id,
                        bool success);

  // Navigates to the New Tab Page.
  void HandleGotoNewTabPage(const base::Value::List& args);

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
  void HandleGetAllRulesets(const base::Value::List& args);

  // Resolves a promise with a JSON object describing the decision for a URL
  // (stay/go) + reason. The result is formatted like this:
  //
  // {
  //   "action": ("stay"|"go"),
  //   "reason": ("globally_disabled"|"protocol"|"sitelist"|...),
  //   "matching_rule": (string|undefined)
  // }
  void HandleGetDecision(const base::Value::List& args);

  // Resolves a promise with the time of the last policy fetch and next policy
  // fetch, as JS timestamps.
  //
  // {
  //   "last_fetch": 123456789,
  //   "next_fetch": 234567890
  // }
  void HandleGetTimestamps(const base::Value::List& args);

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
  void HandleGetRulesetSources(const base::Value::List& args);

  // Immediately re-download and apply XML rules.
  void HandleRefreshXml(const base::Value::List& args);

  // Resolves a promise with the boolean value describing whether the feature
  // is enabled or not which is configured by BrowserSwitcherEnabled key
  void HandleIsBrowserSwitchEnabled(const base::Value::List& args);

  base::CallbackListSubscription prefs_subscription_;

  base::CallbackListSubscription service_subscription_;

  base::WeakPtrFactory<BrowserSwitchHandler> weak_ptr_factory_{this};
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
  web_ui()->RegisterMessageCallback(
      "isBrowserSwitcherEnabled",
      base::BindRepeating(&BrowserSwitchHandler::HandleIsBrowserSwitchEnabled,
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
  prefs_subscription_ = {};
  service_subscription_ = {};
}

void BrowserSwitchHandler::OnAllRulesetsParsed(
    browser_switcher::BrowserSwitcherService* service) {
  SendDataChangedEvent();
}

void BrowserSwitchHandler::OnBrowserSwitcherPrefsChanged(
    browser_switcher::BrowserSwitcherPrefs* prefs,
    const std::vector<std::string>& changed_prefs) {
  SendDataChangedEvent();
}

void BrowserSwitchHandler::SendDataChangedEvent() {
  FireWebUIListener("data-changed");
}

void BrowserSwitchHandler::HandleLaunchAlternativeBrowserAndCloseTab(
    const base::Value::List& args) {
  AllowJavascript();

  std::string callback_id = args[0].GetString();
  std::string url_spec = args[1].GetString();
  GURL url(url_spec);

  auto* service = GetBrowserSwitcherService(web_ui());
  bool should_switch = service->sitelist()->ShouldSwitch(url);
  if (!url.is_valid() || !should_switch) {
    // This URL shouldn't open in an alternative browser. Abort launch, because
    // something weird is going on (e.g. race condition from a new sitelist
    // being loaded).
    RejectJavascriptCallback(args[0], base::Value());
    return;
  }

  service->driver()->TryLaunch(
      url, base::BindOnce(&BrowserSwitchHandler::OnLaunchFinished,
                          weak_ptr_factory_.GetWeakPtr(),
                          base::TimeTicks::Now(), std::move(callback_id)));
}

void BrowserSwitchHandler::OnLaunchFinished(base::TimeTicks start,
                                            std::string callback_id,
                                            bool success) {
  const base::TimeDelta runtime = base::TimeTicks::Now() - start;
  UMA_HISTOGRAM_TIMES("BrowserSwitcher.LaunchTime", runtime);
  UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.LaunchSuccess", success);

  if (!success) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  auto* service = GetBrowserSwitcherService(web_ui());
  auto* profile = Profile::FromWebUI(web_ui());
  // We don't need to resolve the promise, because the tab will close (or
  // navigate to about:newtab) anyways.
  if (service->prefs().KeepLastTab() && IsLastTab(profile)) {
    GotoNewTabPage(web_ui()->GetWebContents());
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&content::WebContents::ClosePage,
                                  web_ui()->GetWebContents()->GetWeakPtr()));
  }
}

void BrowserSwitchHandler::HandleGotoNewTabPage(const base::Value::List& args) {
  GotoNewTabPage(web_ui()->GetWebContents());
}

void BrowserSwitchHandler::HandleGetAllRulesets(const base::Value::List& args) {
  AllowJavascript();
  auto* service = GetBrowserSwitcherService(web_ui());
  auto retval =
      base::Value::Dict()
          .Set("gpo", RuleSetToDict(service->prefs().GetRules()))
          .Set("ieem", RuleSetToDict(*service->sitelist()->GetIeemSitelist()))
          .Set("external_sitelist",
               RuleSetToDict(*service->sitelist()->GetExternalSitelist()))
          .Set("external_greylist",
               RuleSetToDict(*service->sitelist()->GetExternalGreylist()));
  ResolveJavascriptCallback(args[0], retval);
}

void BrowserSwitchHandler::HandleGetDecision(const base::Value::List& args) {
  AllowJavascript();

  GURL url = GURL(args[1].GetString());
  if (!url.is_valid()) {
    RejectJavascriptCallback(args[0], base::Value());
    return;
  }

  auto* service = GetBrowserSwitcherService(web_ui());
  browser_switcher::Decision decision = service->sitelist()->GetDecision(url);

  std::string_view action_name =
      (decision.action == browser_switcher::kStay) ? "stay" : "go";

  std::string_view reason_name;
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

  // clang-format off
  auto retval =
      base::Value::Dict()
          .Set("action", action_name)
          .Set("reason", reason_name);
  // clang-format on

  if (decision.matching_rule) {
    retval.Set("matching_rule", decision.matching_rule->ToString());
  }

  ResolveJavascriptCallback(args[0], retval);
}

void BrowserSwitchHandler::HandleGetTimestamps(const base::Value::List& args) {
  AllowJavascript();

  auto* service = GetBrowserSwitcherService(web_ui());
  auto* downloader = service->sitelist_downloader();

  if (!downloader) {
    ResolveJavascriptCallback(args[0], base::Value());
    return;
  }

  auto retval =
      base::Value::Dict()
          .Set("last_fetch",
               downloader->last_refresh_time().InMillisecondsFSinceUnixEpoch())
          .Set("next_fetch",
               downloader->next_refresh_time().InMillisecondsFSinceUnixEpoch());

  ResolveJavascriptCallback(args[0], retval);
}

void BrowserSwitchHandler::HandleGetRulesetSources(
    const base::Value::List& args) {
  AllowJavascript();

  auto* service = GetBrowserSwitcherService(web_ui());
  std::vector<browser_switcher::RulesetSource> sources =
      service->GetRulesetSources();

  base::Value::Dict retval;
  for (const auto& source : sources) {
    base::Value val;
    if (source.url.is_valid())
      val = base::Value(source.url.spec());
    // |pref_name| is something like "browser_switcher.blah"; however path
    // expansion is not expected on it as the JavaScript expects to see
    // "browser_switcher.blah" as a key in the object, not a nested hierarchy.
    retval.Set(source.pref_name, std::move(val));
  }
  ResolveJavascriptCallback(args[0], retval);
}

void BrowserSwitchHandler::HandleRefreshXml(const base::Value::List& args) {
  auto* service = GetBrowserSwitcherService(web_ui());
  service->StartDownload(base::TimeDelta());
}

void BrowserSwitchHandler::HandleIsBrowserSwitchEnabled(
    const base::Value::List& args) {
  AllowJavascript();

  auto* service = GetBrowserSwitcherService(web_ui());
  ResolveJavascriptCallback(args[0], base::Value(service->prefs().IsEnabled()));
}

BrowserSwitchUI::BrowserSwitchUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<BrowserSwitchHandler>());

  // Set up the chrome://browser-switch source.
  CreateAndAddBrowserSwitchUIHTMLSource(web_ui);
}
