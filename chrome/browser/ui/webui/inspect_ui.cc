// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/inspect_ui.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/devtools/devtools_targets_ui.h"
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/prefs/pref_service.h"
#include "components/ui_devtools/devtools_server.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/frame_navigate_params.h"

using content::DevToolsAgentHost;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

const char kInspectUiInitUICommand[] = "init-ui";
const char kInspectUiInspectCommand[] = "inspect";
const char kInspectUiInspectFallbackCommand[] = "inspect-fallback";
const char kInspectUiInspectAdditionalCommand[] = "inspect-additional";
const char kInspectUiActivateCommand[] = "activate";
const char kInspectUiCloseCommand[] = "close";
const char kInspectUiReloadCommand[] = "reload";
const char kInspectUiOpenCommand[] = "open";
const char kInspectUiPauseCommand[] = "pause";
const char kInspectUiInspectBrowser[] = "inspect-browser";
const char kInspectUiLocalHost[] = "localhost";

const char kInspectUiDiscoverUsbDevicesEnabledCommand[] =
    "set-discover-usb-devices-enabled";
const char kInspectUiPortForwardingEnabledCommand[] =
    "set-port-forwarding-enabled";
const char kInspectUiPortForwardingConfigCommand[] =
    "set-port-forwarding-config";
const char kInspectUiDiscoverTCPTargetsEnabledCommand[] =
    "set-discover-tcp-targets-enabled";
const char kInspectUiTCPDiscoveryConfigCommand[] = "set-tcp-discovery-config";
const char kInspectUiOpenNodeFrontendCommand[] = "open-node-frontend";

const char kInspectUiPortForwardingDefaultPort[] = "8080";
const char kInspectUiPortForwardingDefaultLocation[] = "localhost:8080";

const char kInspectUiNameField[] = "name";
const char kInspectUiUrlField[] = "url";
const char kInspectUiIsAdditionalField[] = "isAdditional";

base::Value GetUiDevToolsTargets() {
  base::Value targets(base::Value::Type::LIST);
  for (const auto& client_pair :
       ui_devtools::UiDevToolsServer::GetClientNamesAndUrls()) {
    base::Value target_data(base::Value::Type::DICTIONARY);
    target_data.SetStringKey(kInspectUiNameField, client_pair.first);
    target_data.SetStringKey(kInspectUiUrlField, client_pair.second);
    target_data.SetBoolKey(kInspectUiIsAdditionalField, true);
    targets.Append(std::move(target_data));
  }
  return targets;
}

// InspectMessageHandler --------------------------------------------

class InspectMessageHandler : public WebUIMessageHandler {
 public:
  explicit InspectMessageHandler(InspectUI* inspect_ui)
      : inspect_ui_(inspect_ui) {}
  ~InspectMessageHandler() override {}

 private:
  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  void HandleInitUICommand(const base::ListValue* args);
  void HandleInspectCommand(const base::ListValue* args);
  void HandleInspectFallbackCommand(const base::ListValue* args);
  void HandleInspectAdditionalCommand(const base::ListValue* args);
  void HandleActivateCommand(const base::ListValue* args);
  void HandleCloseCommand(const base::ListValue* args);
  void HandleReloadCommand(const base::ListValue* args);
  void HandleOpenCommand(const base::ListValue* args);
  void HandlePauseCommand(const base::ListValue* args);
  void HandleInspectBrowserCommand(const base::ListValue* args);
  void HandleBooleanPrefChanged(const char* pref_name,
                                const base::ListValue* args);
  void HandlePortForwardingConfigCommand(const base::ListValue* args);
  void HandleTCPDiscoveryConfigCommand(const base::ListValue* args);
  void HandleOpenNodeFrontendCommand(const base::ListValue* args);

  InspectUI* const inspect_ui_;

  DISALLOW_COPY_AND_ASSIGN(InspectMessageHandler);
};

void InspectMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kInspectUiInitUICommand,
      base::BindRepeating(&InspectMessageHandler::HandleInitUICommand,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kInspectUiInspectCommand,
      base::BindRepeating(&InspectMessageHandler::HandleInspectCommand,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kInspectUiInspectFallbackCommand,
      base::BindRepeating(&InspectMessageHandler::HandleInspectFallbackCommand,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kInspectUiInspectAdditionalCommand,
      base::BindRepeating(
          &InspectMessageHandler::HandleInspectAdditionalCommand,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kInspectUiActivateCommand,
      base::BindRepeating(&InspectMessageHandler::HandleActivateCommand,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kInspectUiCloseCommand,
      base::BindRepeating(&InspectMessageHandler::HandleCloseCommand,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kInspectUiPauseCommand,
      base::BindRepeating(&InspectMessageHandler::HandlePauseCommand,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kInspectUiDiscoverUsbDevicesEnabledCommand,
      base::BindRepeating(&InspectMessageHandler::HandleBooleanPrefChanged,
                          base::Unretained(this),
                          &prefs::kDevToolsDiscoverUsbDevicesEnabled[0]));
  web_ui()->RegisterMessageCallback(
      kInspectUiPortForwardingEnabledCommand,
      base::BindRepeating(&InspectMessageHandler::HandleBooleanPrefChanged,
                          base::Unretained(this),
                          &prefs::kDevToolsPortForwardingEnabled[0]));
  web_ui()->RegisterMessageCallback(
      kInspectUiPortForwardingConfigCommand,
      base::BindRepeating(
          &InspectMessageHandler::HandlePortForwardingConfigCommand,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kInspectUiDiscoverTCPTargetsEnabledCommand,
      base::BindRepeating(&InspectMessageHandler::HandleBooleanPrefChanged,
                          base::Unretained(this),
                          &prefs::kDevToolsDiscoverTCPTargetsEnabled[0]));
  web_ui()->RegisterMessageCallback(
      kInspectUiTCPDiscoveryConfigCommand,
      base::BindRepeating(
          &InspectMessageHandler::HandleTCPDiscoveryConfigCommand,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kInspectUiOpenNodeFrontendCommand,
      base::BindRepeating(&InspectMessageHandler::HandleOpenNodeFrontendCommand,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kInspectUiReloadCommand,
      base::BindRepeating(&InspectMessageHandler::HandleReloadCommand,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kInspectUiOpenCommand,
      base::BindRepeating(&InspectMessageHandler::HandleOpenCommand,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kInspectUiInspectBrowser,
      base::BindRepeating(&InspectMessageHandler::HandleInspectBrowserCommand,
                          base::Unretained(this)));
}

void InspectMessageHandler::HandleInitUICommand(const base::ListValue*) {
  inspect_ui_->InitUI();
}

static bool ParseStringArgs(const base::ListValue* args,
                            std::string* arg0,
                            std::string* arg1,
                            std::string* arg2 = 0) {
  int arg_size = args->GetSize();
  return (!arg0 || (arg_size > 0 && args->GetString(0, arg0))) &&
         (!arg1 || (arg_size > 1 && args->GetString(1, arg1))) &&
         (!arg2 || (arg_size > 2 && args->GetString(2, arg2)));
}

void InspectMessageHandler::HandleInspectCommand(const base::ListValue* args) {
  std::string source;
  std::string id;
  if (ParseStringArgs(args, &source, &id))
    inspect_ui_->Inspect(source, id);
}

void InspectMessageHandler::HandleInspectFallbackCommand(
    const base::ListValue* args) {
  std::string source;
  std::string id;
  if (ParseStringArgs(args, &source, &id))
    inspect_ui_->InspectFallback(source, id);
}

void InspectMessageHandler::HandleInspectAdditionalCommand(
    const base::ListValue* args) {
  std::string url;
  if (ParseStringArgs(args, &url, nullptr)) {
    WebContents* inspect_ui = web_ui()->GetWebContents();
    web_ui()->GetWebContents()->GetDelegate()->OpenURLFromTab(
        inspect_ui,
        content::OpenURLParams(GURL(url), content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false));
  }
}

void InspectMessageHandler::HandleActivateCommand(const base::ListValue* args) {
  std::string source;
  std::string id;
  if (ParseStringArgs(args, &source, &id))
    inspect_ui_->Activate(source, id);
}

void InspectMessageHandler::HandleCloseCommand(const base::ListValue* args) {
  std::string source;
  std::string id;
  if (ParseStringArgs(args, &source, &id))
    inspect_ui_->Close(source, id);
}

void InspectMessageHandler::HandleReloadCommand(const base::ListValue* args) {
  std::string source;
  std::string id;
  if (ParseStringArgs(args, &source, &id))
    inspect_ui_->Reload(source, id);
}

void InspectMessageHandler::HandleOpenCommand(const base::ListValue* args) {
  std::string source_id;
  std::string browser_id;
  std::string url;
  if (ParseStringArgs(args, &source_id, &browser_id, &url))
    inspect_ui_->Open(source_id, browser_id, url);
}

void InspectMessageHandler::HandlePauseCommand(const base::ListValue* args) {
  std::string source;
  std::string id;
  if (ParseStringArgs(args, &source, &id))
    inspect_ui_->Pause(source, id);
}

void InspectMessageHandler::HandleInspectBrowserCommand(
    const base::ListValue* args) {
  std::string source_id;
  std::string browser_id;
  std::string front_end;
  if (ParseStringArgs(args, &source_id, &browser_id, &front_end)) {
    inspect_ui_->InspectBrowserWithCustomFrontend(
        source_id, browser_id, GURL(front_end));
  }
}

void InspectMessageHandler::HandleBooleanPrefChanged(
    const char* pref_name,
    const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;

  bool enabled;
  if (args->GetSize() == 1 && args->GetBoolean(0, &enabled))
    profile->GetPrefs()->SetBoolean(pref_name, enabled);
}

void InspectMessageHandler::HandlePortForwardingConfigCommand(
    const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;

  const base::DictionaryValue* dict_src;
  if (args->GetSize() == 1 && args->GetDictionary(0, &dict_src))
    profile->GetPrefs()->Set(prefs::kDevToolsPortForwardingConfig, *dict_src);
}

void InspectMessageHandler::HandleTCPDiscoveryConfigCommand(
    const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;

  const base::ListValue* list_src;
  if (args->GetSize() == 1 && args->GetList(0, &list_src))
    profile->GetPrefs()->Set(prefs::kDevToolsTCPDiscoveryConfig, *list_src);
}

void InspectMessageHandler::HandleOpenNodeFrontendCommand(
    const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;
  DevToolsWindow::OpenNodeFrontendWindow(profile);
}

// DevToolsUIBindingsEnabler ----------------------------------------

class DevToolsUIBindingsEnabler
    : public content::WebContentsObserver {
 public:
  DevToolsUIBindingsEnabler(WebContents* web_contents,
                            const GURL& url);
  ~DevToolsUIBindingsEnabler() override {}

  DevToolsUIBindings* GetBindings();

 private:
  // contents::WebContentsObserver overrides.
  void WebContentsDestroyed() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  DevToolsUIBindings bindings_;
  GURL url_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsUIBindingsEnabler);
};

DevToolsUIBindingsEnabler::DevToolsUIBindingsEnabler(
    WebContents* web_contents,
    const GURL& url)
    : WebContentsObserver(web_contents),
      bindings_(web_contents),
      url_(url) {
}

DevToolsUIBindings* DevToolsUIBindingsEnabler::GetBindings() {
  return &bindings_;
}

void DevToolsUIBindingsEnabler::WebContentsDestroyed() {
  delete this;
}

void DevToolsUIBindingsEnabler::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  if (url_ != navigation_handle->GetURL())
    delete this;
}

}  // namespace

// InspectUI --------------------------------------------------------

InspectUI::InspectUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<InspectMessageHandler>(this));
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateInspectUIHTMLSource());

  // Set up the chrome://theme/ source.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
}

InspectUI::~InspectUI() {
  StopListeningNotifications();
}

void InspectUI::InitUI() {
  SetPortForwardingDefaults();
  StartListeningNotifications();
  UpdateDiscoverUsbDevicesEnabled();
  UpdatePortForwardingEnabled();
  UpdatePortForwardingConfig();
  UpdateTCPDiscoveryEnabled();
  UpdateTCPDiscoveryConfig();
}

void InspectUI::Inspect(const std::string& source_id,
                        const std::string& target_id) {
  scoped_refptr<DevToolsAgentHost> target = FindTarget(source_id, target_id);
  if (target) {
    Profile* profile = Profile::FromWebUI(web_ui());
    DevToolsWindow::OpenDevToolsWindow(target, profile);
  }
}

void InspectUI::InspectFallback(const std::string& source_id,
                                const std::string& target_id) {
  scoped_refptr<DevToolsAgentHost> target = FindTarget(source_id, target_id);
  if (target) {
    Profile* profile = Profile::FromWebUI(web_ui());
    DevToolsWindow::OpenDevToolsWindowWithBundledFrontend(target, profile);
  }
}

void InspectUI::Activate(const std::string& source_id,
                         const std::string& target_id) {
  scoped_refptr<DevToolsAgentHost> target = FindTarget(source_id, target_id);
  if (target)
    target->Activate();
}

void InspectUI::Close(const std::string& source_id,
                      const std::string& target_id) {
  scoped_refptr<DevToolsAgentHost> target = FindTarget(source_id, target_id);
  if (target) {
    target->Close();
    DevToolsTargetsUIHandler* handler = FindTargetHandler(source_id);
    if (handler)
      handler->ForceUpdate();
  }
}

void InspectUI::Reload(const std::string& source_id,
                       const std::string& target_id) {
  scoped_refptr<DevToolsAgentHost> target = FindTarget(source_id, target_id);
  if (target)
    target->Reload();
}

void InspectUI::Open(const std::string& source_id,
                     const std::string& browser_id,
                     const std::string& url) {
  DevToolsTargetsUIHandler* handler = FindTargetHandler(source_id);
  if (handler)
    handler->Open(browser_id, url);
}

void InspectUI::Pause(const std::string& source_id,
                      const std::string& target_id) {
  scoped_refptr<DevToolsAgentHost> target = FindTarget(source_id, target_id);
  content::WebContents* web_contents = target->GetWebContents();
  if (web_contents) {
    DevToolsWindow::OpenDevToolsWindow(web_contents,
                                       DevToolsToggleAction::PauseInDebugger());
  }
}

void InspectUI::InspectBrowserWithCustomFrontend(
    const std::string& source_id,
    const std::string& browser_id,
    const GURL& frontend_url) {
  if (!frontend_url.SchemeIs(content::kChromeUIScheme) &&
      !frontend_url.SchemeIs(content::kChromeDevToolsScheme) &&
      frontend_url.host() != kInspectUiLocalHost) {
    return;
  }

  DevToolsTargetsUIHandler* handler = FindTargetHandler(source_id);
  if (!handler)
    return;

  // Fetch agent host from remote browser.
  scoped_refptr<content::DevToolsAgentHost> agent_host =
      handler->GetBrowserAgentHost(browser_id);
  if (agent_host->IsAttached())
    return;

  // Create web contents for the front-end.
  WebContents* inspect_ui = web_ui()->GetWebContents();
  WebContents* front_end = inspect_ui->GetDelegate()->OpenURLFromTab(
      inspect_ui,
      content::OpenURLParams(frontend_url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false));

  // Install devtools bindings.
  DevToolsUIBindingsEnabler* bindings_enabler =
      new DevToolsUIBindingsEnabler(front_end, frontend_url);
  bindings_enabler->GetBindings()->AttachTo(agent_host);
}

void InspectUI::InspectDevices(Browser* browser) {
  base::RecordAction(base::UserMetricsAction("InspectDevices"));
  NavigateParams params(GetSingletonTabNavigateParams(
      browser, GURL(chrome::kChromeUIInspectURL)));
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, std::move(params));
}

void InspectUI::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (source == content::Source<WebContents>(web_ui()->GetWebContents()))
    StopListeningNotifications();
}

void InspectUI::StartListeningNotifications() {
  if (!target_handlers_.empty())  // Possible when reloading the page.
    StopListeningNotifications();

  Profile* profile = Profile::FromWebUI(web_ui());

  DevToolsTargetsUIHandler::Callback callback =
      base::Bind(&InspectUI::PopulateTargets, base::Unretained(this));

  PopulateAdditionalTargets(GetUiDevToolsTargets());

  AddTargetUIHandler(
      DevToolsTargetsUIHandler::CreateForLocal(callback, profile));
  if (profile->IsOffTheRecord()) {
    ShowIncognitoWarning();
  } else {
    AddTargetUIHandler(
        DevToolsTargetsUIHandler::CreateForAdb(callback, profile));
  }

  port_status_serializer_.reset(
      new PortForwardingStatusSerializer(
          base::Bind(&InspectUI::PopulatePortStatus, base::Unretained(this)),
          profile));

  notification_registrar_.Add(this,
                              content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                              content::NotificationService::AllSources());

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(prefs::kDevToolsDiscoverUsbDevicesEnabled,
      base::Bind(&InspectUI::UpdateDiscoverUsbDevicesEnabled,
                 base::Unretained(this)));
  pref_change_registrar_.Add(prefs::kDevToolsPortForwardingEnabled,
      base::Bind(&InspectUI::UpdatePortForwardingEnabled,
                 base::Unretained(this)));
  pref_change_registrar_.Add(prefs::kDevToolsPortForwardingConfig,
      base::Bind(&InspectUI::UpdatePortForwardingConfig,
                 base::Unretained(this)));
  pref_change_registrar_.Add(prefs::kDevToolsDiscoverTCPTargetsEnabled,
      base::Bind(&InspectUI::UpdateTCPDiscoveryEnabled,
                 base::Unretained(this)));
  pref_change_registrar_.Add(prefs::kDevToolsTCPDiscoveryConfig,
      base::Bind(&InspectUI::UpdateTCPDiscoveryConfig,
                 base::Unretained(this)));
}

void InspectUI::StopListeningNotifications() {
  if (target_handlers_.empty())
    return;

  target_handlers_.clear();

  port_status_serializer_.reset();

  notification_registrar_.RemoveAll();
  pref_change_registrar_.RemoveAll();
}

content::WebUIDataSource* InspectUI::CreateInspectUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIInspectHost);
  source->AddResourcePath("inspect.css", IDR_INSPECT_CSS);
  source->AddResourcePath("inspect.js", IDR_INSPECT_JS);
  source->SetDefaultResource(IDR_INSPECT_HTML);
  return source;
}

void InspectUI::UpdateDiscoverUsbDevicesEnabled() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "updateDiscoverUsbDevicesEnabled",
      *GetPrefValue(prefs::kDevToolsDiscoverUsbDevicesEnabled));
}

void InspectUI::UpdatePortForwardingEnabled() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "updatePortForwardingEnabled",
      *GetPrefValue(prefs::kDevToolsPortForwardingEnabled));
}

void InspectUI::UpdatePortForwardingConfig() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "updatePortForwardingConfig",
      *GetPrefValue(prefs::kDevToolsPortForwardingConfig));
}

void InspectUI::UpdateTCPDiscoveryEnabled() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "updateTCPDiscoveryEnabled",
      *GetPrefValue(prefs::kDevToolsDiscoverTCPTargetsEnabled));
}

void InspectUI::UpdateTCPDiscoveryConfig() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "updateTCPDiscoveryConfig",
      *GetPrefValue(prefs::kDevToolsTCPDiscoveryConfig));
}

void InspectUI::SetPortForwardingDefaults() {
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

  bool default_set;
  if (!GetPrefValue(prefs::kDevToolsPortForwardingDefaultSet)->
      GetAsBoolean(&default_set) || default_set) {
    return;
  }

  // This is the first chrome://inspect invocation on a fresh profile or after
  // upgrade from a version that did not have kDevToolsPortForwardingDefaultSet.
  prefs->SetBoolean(prefs::kDevToolsPortForwardingDefaultSet, true);

  bool enabled;
  const base::DictionaryValue* config;
  if (!GetPrefValue(prefs::kDevToolsPortForwardingEnabled)->
        GetAsBoolean(&enabled) ||
      !GetPrefValue(prefs::kDevToolsPortForwardingConfig)->
        GetAsDictionary(&config)) {
    return;
  }

  // Do nothing if user already took explicit action.
  if (enabled || !config->empty())
    return;

  base::DictionaryValue default_config;
  default_config.SetString(kInspectUiPortForwardingDefaultPort,
                           kInspectUiPortForwardingDefaultLocation);
  prefs->Set(prefs::kDevToolsPortForwardingConfig, default_config);
}

const base::Value* InspectUI::GetPrefValue(const char* name) {
  Profile* profile = Profile::FromWebUI(web_ui());
  return profile->GetPrefs()->FindPreference(name)->GetValue();
}

void InspectUI::AddTargetUIHandler(
    std::unique_ptr<DevToolsTargetsUIHandler> handler) {
  std::string id = handler->source_id();
  target_handlers_[id] = std::move(handler);
}

DevToolsTargetsUIHandler* InspectUI::FindTargetHandler(
    const std::string& source_id) {
  auto it = target_handlers_.find(source_id);
  return it != target_handlers_.end() ? it->second.get() : nullptr;
}

scoped_refptr<content::DevToolsAgentHost> InspectUI::FindTarget(
    const std::string& source_id, const std::string& target_id) {
  auto it = target_handlers_.find(source_id);
  return it != target_handlers_.end() ?
      it->second->GetTarget(target_id) : nullptr;
}

void InspectUI::PopulateTargets(const std::string& source,
                                const base::ListValue& targets) {
  web_ui()->CallJavascriptFunctionUnsafe("populateTargets", base::Value(source),
                                         targets);
}

void InspectUI::PopulateAdditionalTargets(const base::Value& targets) {
  web_ui()->CallJavascriptFunctionUnsafe("populateAdditionalTargets", targets);
}

void InspectUI::PopulatePortStatus(const base::Value& status) {
  web_ui()->CallJavascriptFunctionUnsafe("populatePortStatus", status);
}

void InspectUI::ShowIncognitoWarning() {
  web_ui()->CallJavascriptFunctionUnsafe("showIncognitoWarning");
}
