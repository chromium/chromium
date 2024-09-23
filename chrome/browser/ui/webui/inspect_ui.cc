// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/inspect_ui.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "base/version.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/devtools/devtools_targets_ui.h"
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/prefs/pref_service.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/switches.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/widget/widget.h"

using content::DevToolsAgentHost;
using content::WebContents;
using content::WebUIMessageHandler;

namespace ui_devtools {

// This class is a friend of views::Widget.
class BubbleLocking {
 public:
  static void SetEnabled(bool enabled) {
    views::Widget::SetDisableActivationChangeHandling(
        enabled ? views::Widget::DisableActivationChangeHandlingType::
                      kIgnoreDeactivationOnly
                : views::Widget::DisableActivationChangeHandlingType::kNone);
  }

  static bool GetEnabled() {
    return views::Widget::GetDisableActivationChangeHandling() !=
           views::Widget::DisableActivationChangeHandlingType::kNone;
  }

 private:
  BubbleLocking() = default;
};

}  // namespace ui_devtools

namespace {

const char kInspectUiInitUICommand[] = "init-ui";
const char kInspectUiInspectCommand[] = "inspect";
const char kInspectUiInspectFallbackCommand[] = "inspect-fallback";
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
const char kInspectUiBubbleLockingCommand[] = "set-bubble-locking";
const char kInspectUiTCPDiscoveryConfigCommand[] = "set-tcp-discovery-config";
const char kInspectUiOpenNodeFrontendCommand[] = "open-node-frontend";
const char kInspectUiLaunchUIDevToolsCommand[] = "launch-ui-devtools";

const char kInspectUiPortForwardingDefaultPort[] = "8080";
const char kInspectUiPortForwardingDefaultLocation[] = "localhost:8080";

const char kInspectUiNameField[] = "name";
const char kInspectUiUrlField[] = "url";
const char kInspectUiIsNativeField[] = "isNative";

base::Value::List GetUiDevToolsTargets() {
  base::Value::List targets;
  for (const auto& client_pair :
       ui_devtools::UiDevToolsServer::GetClientNamesAndUrls()) {
    base::Value::Dict target_data;
    target_data.Set(kInspectUiNameField, client_pair.first);
    target_data.Set(kInspectUiUrlField, client_pair.second);
    target_data.Set(kInspectUiIsNativeField, true);
    targets.Append(std::move(target_data));
  }
  return targets;
}

void CreateAndAddInspectUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIInspectHost);
  source->AddResourcePath("inspect.css", IDR_INSPECT_CSS);
  source->AddResourcePath("inspect.js", IDR_INSPECT_JS);
  source->SetDefaultResource(IDR_INSPECT_HTML);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");
}

// DevToolsFrontEndObserver ----------------------------------------
// Owned by the WebContents passed in.
class DevToolsFrontEndObserver : public content::WebContentsObserver {
 public:
  DevToolsFrontEndObserver(WebContents* web_contents,
                           base::OnceClosure closure);

  ~DevToolsFrontEndObserver() override;

  DevToolsFrontEndObserver(const DevToolsFrontEndObserver&) = delete;
  DevToolsFrontEndObserver& operator=(const DevToolsFrontEndObserver&) = delete;

 private:
  // contents::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;

  bool front_end_page_committed_ = false;

  // Callback function executed when the front end is finished.
  base::OnceClosure on_front_end_finished_;
};

DevToolsFrontEndObserver::DevToolsFrontEndObserver(WebContents* web_contents,
                                                   base::OnceClosure closure)
    : WebContentsObserver(web_contents),
      on_front_end_finished_(std::move(closure)) {
  DCHECK(web_contents);
}

DevToolsFrontEndObserver::~DevToolsFrontEndObserver() {
  if (!on_front_end_finished_.is_null()) {
    std::move(on_front_end_finished_).Run();
  }
}

void DevToolsFrontEndObserver::PrimaryPageChanged(content::Page& page) {
  if (!front_end_page_committed_ && !page.GetMainDocument().IsErrorDocument()) {
    front_end_page_committed_ = true;
    return;
  }
  delete this;
}

void DevToolsFrontEndObserver::WebContentsDestroyed() {
  delete this;
}

// DevToolsUIBindingsEnabler ----------------------------------------

class DevToolsUIBindingsEnabler : public DevToolsFrontEndObserver {
 public:
  DevToolsUIBindingsEnabler(WebContents* web_contents, const GURL& url);
  DevToolsUIBindingsEnabler(const DevToolsUIBindingsEnabler&) = delete;
  DevToolsUIBindingsEnabler& operator=(const DevToolsUIBindingsEnabler&) =
      delete;
  ~DevToolsUIBindingsEnabler() override = default;

  DevToolsUIBindings* GetBindings();

 private:
  DevToolsUIBindings bindings_;
};

DevToolsUIBindingsEnabler::DevToolsUIBindingsEnabler(WebContents* web_contents,
                                                     const GURL& url)
    : DevToolsFrontEndObserver(web_contents, base::NullCallback()),
      bindings_(web_contents) {}

DevToolsUIBindings* DevToolsUIBindingsEnabler::GetBindings() {
  return &bindings_;
}

// InspectMessageHandler --------------------------------------------

class InspectMessageHandler : public WebUIMessageHandler {
 public:
  explicit InspectMessageHandler(InspectUI* inspect_ui)
      : inspect_ui_(inspect_ui) {}
  InspectMessageHandler(const InspectMessageHandler&) = delete;
  InspectMessageHandler& operator=(const InspectMessageHandler&) = delete;
  ~InspectMessageHandler() override = default;

 private:
  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  void HandleInitUICommand(const base::Value::List& args);
  void HandleInspectCommand(const base::Value::List& args);
  void HandleInspectFallbackCommand(const base::Value::List& args);
  void HandleActivateCommand(const base::Value::List& args);
  void HandleCloseCommand(const base::Value::List& args);
  void HandleReloadCommand(const base::Value::List& args);
  void HandleOpenCommand(const base::Value::List& args);
  void HandlePauseCommand(const base::Value::List& args);
  void HandleInspectBrowserCommand(const base::Value::List& args);
  void HandleBooleanPrefChanged(const char* pref_name,
                                const base::Value::List& args);
  void HandlePortForwardingConfigCommand(const base::Value::List& args);
  void HandleTCPDiscoveryConfigCommand(const base::Value::List& args);
  void HandleOpenNodeFrontendCommand(const base::Value::List& args);
  void HandleLaunchUIDevToolsCommand(const base::Value::List& args);
  void HandleSetBubbleLocking(const base::Value::List& args);

  void CreateNativeUIInspectionSession(const std::string& url);
  void OnFrontEndFinished();

  const raw_ptr<InspectUI, DanglingUntriaged> inspect_ui_;

  base::WeakPtrFactory<InspectMessageHandler> weak_factory_{this};
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
      kInspectUiLaunchUIDevToolsCommand,
      base::BindRepeating(&InspectMessageHandler::HandleLaunchUIDevToolsCommand,
                          base::Unretained(this)));
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
  web_ui()->RegisterMessageCallback(
      kInspectUiBubbleLockingCommand,
      base::BindRepeating(&InspectMessageHandler::HandleSetBubbleLocking,
                          base::Unretained(this)));
}

void InspectMessageHandler::HandleInitUICommand(const base::Value::List&) {
  inspect_ui_->InitUI();
}

static bool ParseStringArgs(const base::Value::List& args,
                            std::string* arg0,
                            std::string* arg1,
                            std::string* arg2 = nullptr) {
  int arg_size = args.size();
  if (arg0) {
    if (arg_size < 1 || !args[0].is_string()) {
      return false;
    }
    *arg0 = args[0].GetString();
  }
  if (arg1) {
    if (arg_size < 2 || !args[1].is_string()) {
      return false;
    }
    *arg1 = args[1].GetString();
  }
  if (arg2) {
    if (arg_size < 3 || !args[2].is_string()) {
      return false;
    }
    *arg2 = args[2].GetString();
  }
  return true;
}

void InspectMessageHandler::HandleInspectCommand(
    const base::Value::List& args) {
  std::string source;
  std::string id;
  if (ParseStringArgs(args, &source, &id))
    inspect_ui_->Inspect(source, id);
}

void InspectMessageHandler::HandleInspectFallbackCommand(
    const base::Value::List& args) {
  std::string source;
  std::string id;
  if (ParseStringArgs(args, &source, &id))
    inspect_ui_->InspectFallback(source, id);
}

void InspectMessageHandler::HandleActivateCommand(
    const base::Value::List& args) {
  std::string source;
  std::string id;
  if (ParseStringArgs(args, &source, &id))
    inspect_ui_->Activate(source, id);
}

void InspectMessageHandler::HandleCloseCommand(const base::Value::List& args) {
  std::string source;
  std::string id;
  if (ParseStringArgs(args, &source, &id))
    inspect_ui_->Close(source, id);
}

void InspectMessageHandler::HandleReloadCommand(const base::Value::List& args) {
  std::string source;
  std::string id;
  if (ParseStringArgs(args, &source, &id))
    inspect_ui_->Reload(source, id);
}

void InspectMessageHandler::HandleOpenCommand(const base::Value::List& args) {
  std::string source_id;
  std::string browser_id;
  std::string url;
  if (ParseStringArgs(args, &source_id, &browser_id, &url))
    inspect_ui_->Open(source_id, browser_id, url);
}

void InspectMessageHandler::HandlePauseCommand(const base::Value::List& args) {
  std::string source;
  std::string id;
  if (ParseStringArgs(args, &source, &id))
    inspect_ui_->Pause(source, id);
}

void InspectMessageHandler::HandleInspectBrowserCommand(
    const base::Value::List& args) {
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
    const base::Value::List& args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;

  if (args.size() == 1 && args[0].is_bool())
    profile->GetPrefs()->SetBoolean(pref_name, args[0].GetBool());
}

void InspectMessageHandler::HandlePortForwardingConfigCommand(
    const base::Value::List& args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;

  if (args.size() == 1) {
    const base::Value& src = args[0];
    if (src.is_dict())
      profile->GetPrefs()->Set(prefs::kDevToolsPortForwardingConfig, src);
  }
}

void InspectMessageHandler::HandleTCPDiscoveryConfigCommand(
    const base::Value::List& args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;

  if (args.size() == 1u && args[0].is_list())
    profile->GetPrefs()->Set(prefs::kDevToolsTCPDiscoveryConfig, args[0]);
}

void InspectMessageHandler::HandleOpenNodeFrontendCommand(
    const base::Value::List& args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;
  DevToolsWindow::OpenNodeFrontendWindow(profile,
                                         DevToolsOpenedByAction::kInspectLink);
}

void InspectMessageHandler::HandleLaunchUIDevToolsCommand(
    const base::Value::List& args) {
  // Start the UI DevTools server if needed and launch the front-end.
  if (!ChromeBrowserMainExtraPartsViews::Get()->GetUiDevToolsServerInstance()) {
    ChromeBrowserMainExtraPartsViews::Get()->CreateUiDevTools();

    // Make the server only lasts for a session.
    const ui_devtools::UiDevToolsServer* server =
        ChromeBrowserMainExtraPartsViews::Get()->GetUiDevToolsServerInstance();
    server->SetOnSessionEnded(base::BindOnce([]() {
      if (ChromeBrowserMainExtraPartsViews::Get()
              ->GetUiDevToolsServerInstance())
        ChromeBrowserMainExtraPartsViews::Get()->DestroyUiDevTools();
    }));
  }
  inspect_ui_->PopulateNativeUITargets(GetUiDevToolsTargets());

  std::vector<ui_devtools::UiDevToolsServer::NameUrlPair> pairs =
      ui_devtools::UiDevToolsServer::GetClientNamesAndUrls();
  if (!pairs.empty())
    CreateNativeUIInspectionSession(pairs[0].second);
}

void InspectMessageHandler::HandleSetBubbleLocking(
    const base::Value::List& args) {
  CHECK(args.size() == 1 && args[0].is_bool());
  ui_devtools::BubbleLocking::SetEnabled(args[0].GetBool());
}

void InspectMessageHandler::CreateNativeUIInspectionSession(
    const std::string& url) {
  WebContents* inspect_ui = web_ui()->GetWebContents();
  const GURL gurl(url);
  content::WebContents* front_end = inspect_ui->GetDelegate()->OpenURLFromTab(
      inspect_ui,
      content::OpenURLParams(gurl, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false),
      /*navigation_handle_callback=*/{});
  // When the front-end is started, disable the launch button.
  inspect_ui_->ShowNativeUILaunchButton(/* enabled = */ false);

  // The observer will delete itself when the front-end finishes.
  new DevToolsFrontEndObserver(
      front_end, base::BindOnce(&InspectMessageHandler::OnFrontEndFinished,
                                weak_factory_.GetWeakPtr()));
}

void InspectMessageHandler::OnFrontEndFinished() {
  // Clear the client list and re-enable the launch button when the front-end is
  // gone.
  inspect_ui_->PopulateNativeUITargets(base::Value::List());
  inspect_ui_->ShowNativeUILaunchButton(/* enabled = */ true);
}

}  // namespace

// InspectUI --------------------------------------------------------

InspectUI::InspectUI(content::WebUI* web_ui)
    : WebUIController(web_ui), WebContentsObserver(web_ui->GetWebContents()) {
  web_ui->AddMessageHandler(std::make_unique<InspectMessageHandler>(this));
  Profile* profile = Profile::FromWebUI(web_ui);
  CreateAndAddInspectUIHTMLSource(profile);

  // Set up the chrome://theme/ source.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
}

InspectUI::~InspectUI() {
  StopListeningNotifications();
}

void InspectUI::InitUI() {
  SetHostVersion(version_info::GetVersion().GetString());
  SetPortForwardingDefaults();
  StartListeningNotifications();
  UpdateDiscoverUsbDevicesEnabled();
  UpdatePortForwardingEnabled();
  UpdatePortForwardingConfig();
  UpdateTCPDiscoveryEnabled();
  UpdateTCPDiscoveryConfig();
  UpdateBubbleLockingCheckbox();
}

void InspectUI::Inspect(const std::string& source_id,
                        const std::string& target_id) {
  scoped_refptr<DevToolsAgentHost> target = FindTarget(source_id, target_id);
  if (target) {
    Profile* profile = Profile::FromWebUI(web_ui());
    DevToolsWindow::OpenDevToolsWindow(target, profile,
                                       DevToolsOpenedByAction::kInspectLink);
  }
}

void InspectUI::InspectFallback(const std::string& source_id,
                                const std::string& target_id) {
  scoped_refptr<DevToolsAgentHost> target = FindTarget(source_id, target_id);
  if (target) {
    Profile* profile = Profile::FromWebUI(web_ui());
    DevToolsWindow::OpenDevToolsWindowWithBundledFrontend(
        target, profile, DevToolsOpenedByAction::kInspectLink);
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
                                       DevToolsToggleAction::PauseInDebugger(),
                                       DevToolsOpenedByAction::kInspectLink);
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
                             ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false),
      /*navigation_handle_callback=*/{});

  // Install devtools bindings.
  DevToolsUIBindingsEnabler* bindings_enabler =
      new DevToolsUIBindingsEnabler(front_end, frontend_url);
  bindings_enabler->GetBindings()->AttachTo(agent_host);
}

void InspectUI::InspectDevices(Browser* browser) {
  base::RecordAction(base::UserMetricsAction("InspectDevices"));
  ShowSingletonTabOverwritingNTP(browser, GURL(chrome::kChromeUIInspectURL),
                                 NavigateParams::IGNORE_AND_NAVIGATE);
}

void InspectUI::WebContentsDestroyed() {
  StopListeningNotifications();
}

void InspectUI::StartListeningNotifications() {
  if (!target_handlers_.empty())  // Possible when reloading the page.
    StopListeningNotifications();

  Profile* profile = Profile::FromWebUI(web_ui());

  DevToolsTargetsUIHandler::Callback callback =
      base::BindRepeating(&InspectUI::PopulateTargets, base::Unretained(this));

  // Show native UI launch button according to the command line or feature flag.
  if (ui_devtools::UiDevToolsServer::IsUiDevToolsEnabled(
          ui_devtools::switches::kEnableUiDevTools) ||
      base::FeatureList::IsEnabled(features::kUIDebugTools)) {
    ShowNativeUILaunchButton(/* enabled = */ true);
  }

  AddTargetUIHandler(
      DevToolsTargetsUIHandler::CreateForLocal(callback, profile));
  if (profile->IsOffTheRecord()) {
    ShowIncognitoWarning();
  } else {
    AddTargetUIHandler(
        DevToolsTargetsUIHandler::CreateForAdb(callback, profile));
  }

  port_status_serializer_ = std::make_unique<PortForwardingStatusSerializer>(
      base::BindRepeating(&InspectUI::PopulatePortStatus,
                          base::Unretained(this)),
      profile);

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kDevToolsDiscoverUsbDevicesEnabled,
      base::BindRepeating(&InspectUI::UpdateDiscoverUsbDevicesEnabled,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kDevToolsPortForwardingEnabled,
      base::BindRepeating(&InspectUI::UpdatePortForwardingEnabled,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kDevToolsPortForwardingConfig,
      base::BindRepeating(&InspectUI::UpdatePortForwardingConfig,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kDevToolsDiscoverTCPTargetsEnabled,
      base::BindRepeating(&InspectUI::UpdateTCPDiscoveryEnabled,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kDevToolsTCPDiscoveryConfig,
      base::BindRepeating(&InspectUI::UpdateTCPDiscoveryConfig,
                          base::Unretained(this)));
}

void InspectUI::StopListeningNotifications() {
  if (target_handlers_.empty())
    return;

  target_handlers_.clear();

  port_status_serializer_.reset();

  pref_change_registrar_.RemoveAll();
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

void InspectUI::UpdateBubbleLockingCheckbox() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "updateBubbleLockingCheckbox", ui_devtools::BubbleLocking::GetEnabled());
}

void InspectUI::SetPortForwardingDefaults() {
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

  auto default_set =
      GetPrefValue(prefs::kDevToolsPortForwardingDefaultSet)->GetIfBool();
  if (!default_set || default_set.value())
    return;

  // This is the first chrome://inspect invocation on a fresh profile or after
  // upgrade from a version that did not have kDevToolsPortForwardingDefaultSet.
  prefs->SetBoolean(prefs::kDevToolsPortForwardingDefaultSet, true);

  auto enabled =
      GetPrefValue(prefs::kDevToolsPortForwardingEnabled)->GetIfBool();
  if (!enabled)
    return;

  const base::Value::Dict* config =
      GetPrefValue(prefs::kDevToolsPortForwardingConfig)->GetIfDict();
  if (!config)
    return;

  // Do nothing if user already took explicit action.
  if (enabled.value() || !config->empty())
    return;

  base::Value::Dict default_config;
  default_config.Set(kInspectUiPortForwardingDefaultPort,
                     kInspectUiPortForwardingDefaultLocation);
  prefs->SetDict(prefs::kDevToolsPortForwardingConfig,
                 std::move(default_config));
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
                                const base::Value& targets) {
  web_ui()->CallJavascriptFunctionUnsafe("populateTargets", base::Value(source),
                                         targets);
}

void InspectUI::PopulateNativeUITargets(const base::Value::List& targets) {
  web_ui()->CallJavascriptFunctionUnsafe("populateNativeUITargets", targets);
}

void InspectUI::PopulatePortStatus(base::Value status) {
  web_ui()->CallJavascriptFunctionUnsafe("populatePortStatus",
                                         std::move(status));
}

void InspectUI::ShowIncognitoWarning() {
  web_ui()->CallJavascriptFunctionUnsafe("showIncognitoWarning");
}

void InspectUI::ShowNativeUILaunchButton(bool enabled) {
  web_ui()->CallJavascriptFunctionUnsafe("showNativeUILaunchButton",
                                         base::Value(enabled));
}

void InspectUI::SetHostVersion(const std::string& source) {
  web_ui()->CallJavascriptFunctionUnsafe("setHostVersion", base::Value(source));
}
