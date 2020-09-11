// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session_commands.h"

#include <list>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/logging.h"  // For CHECK macros.
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/test/chromedriver/basic_types.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/chrome_android_impl.h"
#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"
#include "chrome/test/chromedriver/chrome/device_manager.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/geoposition.h"
#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/chrome_launcher.h"
#include "chrome/test/chromedriver/command_listener.h"
#include "chrome/test/chromedriver/constants/version.h"
#include "chrome/test/chromedriver/logging.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/util.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace {

const int kWifiMask = 0x2;
const int k4GMask = 0x8;
const int k3GMask = 0x10;
const int k2GMask = 0x20;

const int kAirplaneModeLatency = 0;
const int kAirplaneModeThroughput = 0;
const int kWifiLatency = 2;
const int kWifiThroughput = 30720 * 1024;
const int k4GLatency = 20;
const int k4GThroughput = 4096 * 1024;
const int k3GLatency = 100;
const int k3GThroughput = 750 * 1024;
const int k2GLatency = 300;
const int k2GThroughput = 250 * 1024;

Status EvaluateScriptAndIgnoreResult(Session* session,
                                     std::string expression,
                                     const bool awaitPromise = false) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;
  if (web_view->GetJavaScriptDialogManager()->IsDialogOpen()) {
    std::string alert_text;
    status =
        web_view->GetJavaScriptDialogManager()->GetDialogMessage(&alert_text);
    if (status.IsError())
      return Status(kUnexpectedAlertOpen);
    return Status(kUnexpectedAlertOpen, "{Alert text : " + alert_text + "}");
  }
  std::string frame_id = session->GetCurrentFrameId();
  std::unique_ptr<base::Value> result;
  return web_view->EvaluateScript(frame_id, expression, awaitPromise, &result);
}

void InitSessionForWebSocketConnection(SessionConnectionMap* session_map,
                                       std::string session_id) {
  session_map->insert({session_id, -1});
}

}  // namespace

InitSessionParams::InitSessionParams(
    network::mojom::URLLoaderFactory* factory,
    const SyncWebSocketFactory& socket_factory,
    DeviceManager* device_manager,
    const scoped_refptr<base::SingleThreadTaskRunner> cmd_task_runner,
    SessionConnectionMap* session_map)
    : url_loader_factory(factory),
      socket_factory(socket_factory),
      device_manager(device_manager),
      cmd_task_runner(cmd_task_runner),
      session_map(session_map) {}

InitSessionParams::InitSessionParams(const InitSessionParams& other) = default;

InitSessionParams::~InitSessionParams() {}

// Look for W3C mode setting in InitSession command parameters.
bool GetW3CSetting(const base::DictionaryValue& params) {
  bool w3c;
  const base::ListValue* list;
  const base::DictionaryValue* caps_dict;
  const base::DictionaryValue* options_dict;

  if (params.GetDictionary("capabilities.alwaysMatch", &caps_dict)) {
    if (GetChromeOptionsDictionary(*caps_dict, &options_dict) &&
        options_dict->GetBoolean("w3c", &w3c)) {
      return w3c;
    }
  }

  if (params.GetList("capabilities.firstMatch", &list) &&
      list->GetDictionary(0, &caps_dict)) {
    if (GetChromeOptionsDictionary(*caps_dict, &options_dict) &&
        options_dict->GetBoolean("w3c", &w3c)) {
      return w3c;
    }
  }

  if (params.GetDictionary("desiredCapabilities", &caps_dict)) {
    if (GetChromeOptionsDictionary(*caps_dict, &options_dict) &&
        options_dict->GetBoolean("w3c", &w3c)) {
      return w3c;
    }
  }

  if (!params.HasKey("capabilities") && params.HasKey("desiredCapabilities")) {
    return false;
  }

  return kW3CDefault;
}

namespace {

// Creates a JSON object (represented by base::DictionaryValue) that contains
// the capabilities, for returning to the client app as the result of New
// Session command.
std::unique_ptr<base::DictionaryValue> CreateCapabilities(
    Session* session,
    const Capabilities& capabilities,
    const base::DictionaryValue& desired_caps) {
  std::unique_ptr<base::DictionaryValue> caps(new base::DictionaryValue());

  // Capabilities defined by W3C. Some of these capabilities have different
  // names in legacy mode.
  caps->SetString("browserName", base::ToLowerASCII(kBrowserShortName));
  caps->SetString(session->w3c_compliant ? "browserVersion" : "version",
                  session->chrome->GetBrowserInfo()->browser_version);
  std::string operatingSystemName = session->chrome->GetOperatingSystemName();
  if (operatingSystemName.find("Windows") != std::string::npos)
    operatingSystemName = "Windows";
  if (session->w3c_compliant) {
    caps->SetString("platformName", base::ToLowerASCII(operatingSystemName));
  } else {
    caps->SetString("platform", operatingSystemName);
  }
  caps->SetString("pageLoadStrategy", session->chrome->page_load_strategy());
  caps->SetBoolean("acceptInsecureCerts", capabilities.accept_insecure_certs);
  const base::Value* proxy = desired_caps.FindKey("proxy");
  if (proxy == nullptr || proxy->is_none())
    caps->SetKey("proxy", base::Value(base::Value::Type::DICTIONARY));
  else
    caps->SetKey("proxy", proxy->Clone());
  // add setWindowRect based on whether we are desktop/android/remote
  if (capabilities.IsAndroid() || capabilities.IsRemoteBrowser()) {
    caps->SetBoolean("setWindowRect", false);
  } else {
    caps->SetBoolean("setWindowRect", true);
  }
  if (session->script_timeout == base::TimeDelta::Max())
    caps->SetPath({"timeouts", "script"}, base::Value());
  else
    SetSafeInt(caps.get(), "timeouts.script",
               session->script_timeout.InMilliseconds());
  SetSafeInt(caps.get(), "timeouts.pageLoad",
             session->page_load_timeout.InMilliseconds());
  SetSafeInt(caps.get(), "timeouts.implicit",
             session->implicit_wait.InMilliseconds());
  caps->SetBoolean("strictFileInteractability",
                    session->strict_file_interactability);
  caps->SetString(session->w3c_compliant ? "unhandledPromptBehavior"
                                         : "unexpectedAlertBehaviour",
                  session->unhandled_prompt_behavior);

  // Extensions defined by the W3C.
  // See https://w3c.github.io/webauthn/#sctn-automation-webdriver-capability
  caps->SetBoolean("webauthn:virtualAuthenticators", !capabilities.IsAndroid());

  // Chrome-specific extensions.
  const std::string chromedriverVersionKey = base::StringPrintf(
      "%s.%sVersion", base::ToLowerASCII(kBrowserShortName).c_str(),
      base::ToLowerASCII(kChromeDriverProductShortName).c_str());
  caps->SetString(chromedriverVersionKey, kChromeDriverVersion);
  const std::string debuggerAddressKey =
      base::StringPrintf("%s.debuggerAddress", kChromeDriverOptionsKeyPrefixed);
  caps->SetString(debuggerAddressKey, session->chrome->GetBrowserInfo()
                                          ->debugger_endpoint.Address()
                                          .ToString());
  ChromeDesktopImpl* desktop = NULL;
  Status status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsOk()) {
    const std::string userDataDirKey = base::StringPrintf(
        "%s.userDataDir", base::ToLowerASCII(kBrowserShortName).c_str());
    caps->SetString(userDataDirKey,
                    desktop->command().GetSwitchValueNative("user-data-dir"));
    caps->SetBoolean("networkConnectionEnabled",
                     desktop->IsNetworkConnectionEnabled());
  }

  // Legacy capabilities.
  if (!session->w3c_compliant) {
    caps->SetBoolean("javascriptEnabled", true);
    caps->SetBoolean("takesScreenshot", true);
    caps->SetBoolean("takesHeapSnapshot", true);
    caps->SetBoolean("handlesAlerts", true);
    caps->SetBoolean("databaseEnabled", false);
    caps->SetBoolean("locationContextEnabled", true);
    caps->SetBoolean("mobileEmulationEnabled",
                     session->chrome->IsMobileEmulationEnabled());
    caps->SetBoolean("applicationCacheEnabled", false);
    caps->SetBoolean("browserConnectionEnabled", false);
    caps->SetBoolean("cssSelectorsEnabled", true);
    caps->SetBoolean("webStorageEnabled", true);
    caps->SetBoolean("rotatable", false);
    caps->SetBoolean("acceptSslCerts", capabilities.accept_insecure_certs);
    caps->SetBoolean("nativeEvents", true);
    caps->SetBoolean("hasTouchScreen", session->chrome->HasTouchScreen());
  }

  if (session->webSocketUrl) {
    caps->SetString("webSocketUrl",
                    "ws://" + session->host + "/session/" + session->id);
  }

  return caps;
}

Status CheckSessionCreated(Session* session) {
  WebView* web_view = NULL;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return Status(kSessionNotCreated, status);

  status = web_view->ConnectIfNecessary();
  if (status.IsError())
    return Status(kSessionNotCreated, status);

  base::ListValue args;
  std::unique_ptr<base::Value> result(new base::Value(0));
  status = web_view->CallFunction(session->GetCurrentFrameId(),
                                  "function(s) { return 1; }", args, &result);
  if (status.IsError())
    return Status(kSessionNotCreated, status);

  int response;
  if (!result->GetAsInteger(&response) || response != 1) {
    return Status(kSessionNotCreated,
                  "unexpected response from browser");
  }

  return Status(kOk);
}

Status InitSessionHelper(const InitSessionParams& bound_params,
                         Session* session,
                         const base::DictionaryValue& params,
                         std::unique_ptr<base::Value>* value) {
  const base::DictionaryValue* desired_caps;
  base::DictionaryValue merged_caps;

  Capabilities capabilities;
  Status status = internal::ConfigureSession(session, params, &desired_caps,
                                             &merged_caps, &capabilities);
  if (status.IsError())
    return status;

  // Create Log's and DevToolsEventListener's for ones that are DevTools-based.
  // Session will own the Log's, Chrome will own the listeners.
  // Also create |CommandListener|s for the appropriate logs.
  std::vector<std::unique_ptr<DevToolsEventListener>> devtools_event_listeners;
  std::vector<std::unique_ptr<CommandListener>> command_listeners;
  status = CreateLogs(capabilities,
                      session,
                      &session->devtools_logs,
                      &devtools_event_listeners,
                      &command_listeners);
  if (status.IsError())
    return status;

  // |session| will own the |CommandListener|s.
  session->command_listeners.swap(command_listeners);

  status =
      LaunchChrome(bound_params.url_loader_factory, bound_params.socket_factory,
                   bound_params.device_manager, capabilities,
                   std::move(devtools_event_listeners), &session->chrome,
                   session->w3c_compliant);
  if (status.IsError())
    return status;

  if (capabilities.accept_insecure_certs) {
    status = session->chrome->SetAcceptInsecureCerts();
    if (status.IsError())
      return status;
  }

  status = session->chrome->GetWebViewIdForFirstTab(&session->window,
                                                    session->w3c_compliant);
  if (status.IsError())
    return status;
  session->detach = capabilities.detach;
  session->capabilities =
      CreateCapabilities(session, capabilities, *desired_caps);

  status = internal::ConfigureHeadlessSession(session, capabilities);
  if (status.IsError())
    return status;

  if (session->w3c_compliant) {
    std::unique_ptr<base::DictionaryValue> capabilities =
        std::unique_ptr<base::DictionaryValue>(
            session->capabilities->DeepCopy());
    base::DictionaryValue body;
    body.SetDictionary("capabilities", std::move(capabilities));
    body.SetString("sessionId", session->id);
    value->reset(body.DeepCopy());
  } else {
    value->reset(session->capabilities->DeepCopy());
  }
  return CheckSessionCreated(session);
}

}  // namespace

namespace internal {

Status ConfigureSession(Session* session,
                        const base::DictionaryValue& params,
                        const base::DictionaryValue** desired_caps,
                        base::DictionaryValue* merged_caps,
                        Capabilities* capabilities) {
  session->driver_log.reset(
      new WebDriverLog(WebDriverLog::kDriverType, Log::kAll));

  session->w3c_compliant = GetW3CSetting(params);
  if (session->w3c_compliant) {
    Status status = ProcessCapabilities(params, merged_caps);
    if (status.IsError())
      return status;
    *desired_caps = merged_caps;
  } else if (!params.GetDictionary("desiredCapabilities", desired_caps)) {
    return Status(kSessionNotCreated, "Missing or invalid capabilities");
  }

  Status status = capabilities->Parse(**desired_caps, session->w3c_compliant);
  if (status.IsError())
    return status;

  if (capabilities->unhandled_prompt_behavior.length() > 0) {
    session->unhandled_prompt_behavior =
        capabilities->unhandled_prompt_behavior;
  } else {
    // W3C spec (https://www.w3.org/TR/webdriver/#dfn-handle-any-user-prompts)
    // shows the default behavior to be dismiss and notify. For backward
    // compatibility, in legacy mode default behavior is not handling prompt.
    session->unhandled_prompt_behavior =
        session->w3c_compliant ? kDismissAndNotify : kIgnore;
  }

  session->implicit_wait = capabilities->implicit_wait_timeout;
  session->page_load_timeout = capabilities->page_load_timeout;
  session->script_timeout = capabilities->script_timeout;
  session->strict_file_interactability =
      capabilities->strict_file_interactability;
  session->webSocketUrl = capabilities->webSocketUrl;
  Log::Level driver_level = Log::kWarning;
  if (capabilities->logging_prefs.count(WebDriverLog::kDriverType))
    driver_level = capabilities->logging_prefs[WebDriverLog::kDriverType];
  session->driver_log->set_min_level(driver_level);

  return Status(kOk);
}

Status ConfigureHeadlessSession(Session* session,
                                const Capabilities& capabilities) {
  if (session->chrome->GetBrowserInfo()->is_headless) {
    std::string download_directory;
    if (capabilities.prefs &&
        (capabilities.prefs->GetString("download.default_directory",
                                       &download_directory) ||
         capabilities.prefs->GetStringWithoutPathExpansion(
             "download.default_directory", &download_directory)))
      session->headless_download_directory =
          std::make_unique<std::string>(download_directory);
    else
      session->headless_download_directory = std::make_unique<std::string>(".");
    WebView* first_view;
    session->chrome->GetWebViewById(session->window, &first_view);
    Status status = first_view->OverrideDownloadDirectoryIfNeeded(
        *session->headless_download_directory);
    return status;
  }
  // session is not headless
  return Status(kOk);
}

}  // namespace internal

bool MergeCapabilities(const base::DictionaryValue* always_match,
                       const base::DictionaryValue* first_match,
                       base::DictionaryValue* merged) {
  CHECK(always_match);
  CHECK(first_match);
  CHECK(merged);
  merged->Clear();

  for (base::DictionaryValue::Iterator it(*first_match); !it.IsAtEnd();
       it.Advance()) {
    if (always_match->HasKey(it.key())) {
      // firstMatch cannot have the same |keys| as alwaysMatch.
      return false;
    }
  }

  // merge the capabilities together since guarenteed no key collisions
  merged->MergeDictionary(always_match);
  merged->MergeDictionary(first_match);
  return true;
}

// Implementation of "matching capabilities", as defined in W3C spec at
// https://www.w3.org/TR/webdriver/#dfn-matching-capabilities.
// It checks some requested capabilities and make sure they are supported.
// Currently, we only check "browserName", "platformName", and
// "webauthn:virtualAuthenticators" but more can be added as necessary.
bool MatchCapabilities(const base::DictionaryValue* capabilities) {
  const base::Value* name;
  if (capabilities->Get("browserName", &name) && !name->is_none()) {
    if (!(name->is_string() && name->GetString() == kBrowserCapabilityName))
      return false;
  }

  const base::DictionaryValue* chrome_options;
  const bool has_chrome_options =
      GetChromeOptionsDictionary(*capabilities, &chrome_options);

  bool is_android = has_chrome_options &&
                    chrome_options->FindStringKey("androidPackage") != nullptr;

  const base::Value* platform_name_value;
  if (capabilities->Get("platformName", &platform_name_value) &&
      !platform_name_value->is_none()) {
    if (platform_name_value->is_string()) {
      std::string requested_platform_name = platform_name_value->GetString();
      std::string requested_first_token =
        requested_platform_name.substr(0, requested_platform_name.find(' '));

      std::string actual_platform_name =
        base::ToLowerASCII(base::SysInfo::OperatingSystemName());
      std::string actual_first_token =
        actual_platform_name.substr(0, actual_platform_name.find(' '));

      bool is_remote = has_chrome_options && chrome_options->FindStringKey(
                                                 "debuggerAddress") != nullptr;
      if (requested_platform_name == "any" || is_remote ||
          (is_android && requested_platform_name == "android")) {
        // "any" can be used as a wild card for platformName.
        // if |is_remote| there is no easy way to know
        // target platform. Android check also occurs here.
        // If any of the above cases pass, we return true.
      } else if (is_android && requested_platform_name != "android") {
        return false;
      } else if (requested_first_token == "mac" ||
                 requested_first_token == "windows" ||
                 requested_first_token == "linux") {
        if (actual_first_token != requested_first_token)
          return false;
      } else if (requested_platform_name != actual_platform_name) {
        return false;
      }
    } else {
      return false;
    }
  }

  const base::Value* virtual_authenticators_value;
  if (capabilities->Get("webauthn:virtualAuthenticators",
                        &virtual_authenticators_value) &&
      !virtual_authenticators_value->is_none()) {
    if (!virtual_authenticators_value->is_bool() ||
        (virtual_authenticators_value->GetBool() && is_android)) {
      return false;
    }
  }

  return true;
}

// Implementation of "process capabilities", as defined in W3C spec at
// https://www.w3.org/TR/webdriver/#processing-capabilities. Step numbers in
// the comments correspond to the step numbers in the spec.
Status ProcessCapabilities(const base::DictionaryValue& params,
                           base::DictionaryValue* result_capabilities) {
  // 1. Get the property "capabilities" from parameters.
  const base::DictionaryValue* capabilities_request;
  if (!params.GetDictionary("capabilities", &capabilities_request))
    return Status(kInvalidArgument, "'capabilities' must be a JSON object");

  // 2. Get the property "alwaysMatch" from capabilities request.
  const base::DictionaryValue empty_object;
  const base::DictionaryValue* required_capabilities;
  const base::Value* required_capabilities_value =
      capabilities_request->FindKey("alwaysMatch");
  if (required_capabilities_value == nullptr) {
    required_capabilities = &empty_object;
  } else if (required_capabilities_value->GetAsDictionary(
                 &required_capabilities)) {
    Capabilities cap;
    Status status = cap.Parse(*required_capabilities);
    if (status.IsError())
      return status;
  } else {
    return Status(kInvalidArgument, "'alwaysMatch' must be a JSON object");
  }

  // 3. Get the property "firstMatch" from capabilities request.
  base::ListValue default_list;
  const base::ListValue* all_first_match_capabilities;
  const base::Value* all_first_match_capabilities_value =
      capabilities_request->FindKey("firstMatch");
  if (all_first_match_capabilities_value == nullptr) {
    default_list.Append(std::make_unique<base::DictionaryValue>());
    all_first_match_capabilities = &default_list;
  } else if (all_first_match_capabilities_value->GetAsList(
                 &all_first_match_capabilities)) {
    if (all_first_match_capabilities->GetSize() < 1)
      return Status(kInvalidArgument,
                    "'firstMatch' must contain at least one entry");
  } else {
    return Status(kInvalidArgument, "'firstMatch' must be a JSON list");
  }

  // 4. Let validated first match capabilities be an empty JSON List.
  std::vector<const base::DictionaryValue*> validated_first_match_capabilities;

  // 5. Validate all first match capabilities.
  for (size_t i = 0; i < all_first_match_capabilities->GetSize(); ++i) {
    const base::DictionaryValue* first_match;
    if (!all_first_match_capabilities->GetDictionary(i, &first_match)) {
      return Status(kInvalidArgument,
                    base::StringPrintf(
                        "entry %zu of 'firstMatch' must be a JSON object", i));
    }
    Capabilities cap;
    Status status = cap.Parse(*first_match);
    if (status.IsError())
      return Status(
          kInvalidArgument,
          base::StringPrintf("entry %zu of 'firstMatch' is invalid", i),
          status);
    validated_first_match_capabilities.push_back(first_match);
  }

  // 6. Let merged capabilities be an empty List.
  std::vector<base::DictionaryValue> merged_capabilities;

  // 7. Merge capabilities.
  for (size_t i = 0; i < validated_first_match_capabilities.size(); ++i) {
    const base::DictionaryValue* first_match_capabilities =
        validated_first_match_capabilities[i];
    base::DictionaryValue merged;
    if (!MergeCapabilities(required_capabilities, first_match_capabilities,
                           &merged)) {
      return Status(
          kInvalidArgument,
          base::StringPrintf(
              "unable to merge 'alwaysMatch' with entry %zu of 'firstMatch'",
              i));
    }
    merged_capabilities.emplace_back();
    merged_capabilities.back().Swap(&merged);
  }

  // 8. Match capabilities.
  for (auto& capabilities : merged_capabilities) {
    if (MatchCapabilities(&capabilities)) {
      capabilities.Swap(result_capabilities);
      return Status(kOk);
    }
  }

  // 9. The spec says "return success with data null", but then the caller is
  // instructed to return error when the data is null. Since we don't have a
  // convenient way to return data null, we will take a shortcut and return an
  // error directly.
  return Status(kSessionNotCreated, "No matching capabilities found");
}

Status ExecuteInitSession(const InitSessionParams& bound_params,
                          Session* session,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value) {
  Status status = InitSessionHelper(bound_params, session, params, value);
  if (status.IsError()) {
    session->quit = true;
    if (session->chrome != NULL)
      session->chrome->Quit();
  } else if (session->webSocketUrl) {
    bound_params.cmd_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&InitSessionForWebSocketConnection,
                                  bound_params.session_map, session->id));
  }
  return status;
}

Status ExecuteQuit(bool allow_detach,
                   Session* session,
                   const base::DictionaryValue& params,
                   std::unique_ptr<base::Value>* value) {
  session->quit = true;
  if (allow_detach && session->detach)
    return Status(kOk);
  else
    return session->chrome->Quit();
}

Status ExecuteGetSessionCapabilities(Session* session,
                                     const base::DictionaryValue& params,
                                     std::unique_ptr<base::Value>* value) {
  value->reset(session->capabilities->DeepCopy());
  return Status(kOk);
}

Status ExecuteGetCurrentWindowHandle(Session* session,
                                     const base::DictionaryValue& params,
                                     std::unique_ptr<base::Value>* value) {
  WebView* web_view = NULL;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;
  status = web_view->ConnectIfNecessary();
  if (status.IsError())
    return status;
  value->reset(new base::Value(WebViewIdToWindowHandle(web_view->GetId())));
  return Status(kOk);
}

Status ExecuteClose(Session* session,
                    const base::DictionaryValue& params,
                    std::unique_ptr<base::Value>* value) {
  std::list<std::string> web_view_ids;
  Status status = session->chrome->GetWebViewIds(&web_view_ids,
                                                 session->w3c_compliant);
  if (status.IsError())
    return status;
  bool is_last_web_view = web_view_ids.size() == 1u;
  web_view_ids.clear();

  WebView* web_view = NULL;
  status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  status = web_view->ConnectIfNecessary();
  if (status.IsError())
    return status;

  status = web_view->HandleReceivedEvents();
  if (status.IsError())
    return status;


  JavaScriptDialogManager* dialog_manager =
      web_view->GetJavaScriptDialogManager();
  if (dialog_manager->IsDialogOpen()) {
    std::string alert_text;
    status = dialog_manager->GetDialogMessage(&alert_text);
    if (status.IsError())
      return status;

    // Close the dialog depending on the unexpectedalert behaviour set by user
    // before returning an error, so that subsequent commands do not fail.
    const std::string& prompt_behavior = session->unhandled_prompt_behavior;

    if (prompt_behavior == kAccept || prompt_behavior == kAcceptAndNotify)
      status = dialog_manager->HandleDialog(true, session->prompt_text.get());
    else if (prompt_behavior == kDismiss ||
             prompt_behavior == kDismissAndNotify)
      status = dialog_manager->HandleDialog(false, session->prompt_text.get());
    if (status.IsError())
      return status;

    // For backward compatibility, in legacy mode we always notify.
    if (!session->w3c_compliant || prompt_behavior == kAcceptAndNotify ||
        prompt_behavior == kDismissAndNotify || prompt_behavior == kIgnore)
      return Status(kUnexpectedAlertOpen, "{Alert text : " + alert_text + "}");
  }

  if (!is_last_web_view) {
    status = session->chrome->CloseWebView(web_view->GetId());
    if (status.IsError())
      return status;

    status = ExecuteGetWindowHandles(session, base::DictionaryValue(), value);
    if (status.IsError())
      return status;
  } else {
    // If there is only one open window, close is the same as calling "quit".
    session->quit = true;
    status = session->chrome->Quit();
    if (status.IsOk())
      value->reset(new base::ListValue());
  }

  return status;
}

Status ExecuteGetWindowHandles(Session* session,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value) {
  std::list<std::string> web_view_ids;
  Status status = session->chrome->GetWebViewIds(&web_view_ids,
                                                 session->w3c_compliant);
  if (status.IsError())
    return status;
  std::unique_ptr<base::ListValue> window_ids(new base::ListValue());
  for (std::list<std::string>::const_iterator it = web_view_ids.begin();
       it != web_view_ids.end(); ++it) {
    window_ids->AppendString(WebViewIdToWindowHandle(*it));
  }
  *value = std::move(window_ids);
  return Status(kOk);
}

Status ExecuteSwitchToWindow(Session* session,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value) {
  std::string name;
  if (session->w3c_compliant) {
    if (!params.GetString("handle", &name))
      return Status(kInvalidArgument, "'handle' must be a string");
  } else {
    if (!params.GetString("name", &name))
      return Status(kInvalidArgument, "'name' must be a string");
  }

  std::list<std::string> web_view_ids;
  Status status = session->chrome->GetWebViewIds(&web_view_ids,
                                                 session->w3c_compliant);
  if (status.IsError())
    return status;

  std::string web_view_id;
  bool found = false;
  if (WindowHandleToWebViewId(name, &web_view_id)) {
    // Check if any web_view matches |web_view_id|.
    for (std::list<std::string>::const_iterator it = web_view_ids.begin();
         it != web_view_ids.end(); ++it) {
      if (*it == web_view_id) {
        found = true;
        break;
      }
    }
  } else {
    // Check if any of the tab window names match |name|.
    const char* kGetWindowNameScript = "function() { return window.name; }";
    base::ListValue args;
    for (std::list<std::string>::const_iterator it = web_view_ids.begin();
         it != web_view_ids.end(); ++it) {
      std::unique_ptr<base::Value> result;
      WebView* web_view;
      status = session->chrome->GetWebViewById(*it, &web_view);
      if (status.IsError())
        return status;
      status = web_view->ConnectIfNecessary();
      if (status.IsError())
        return status;
      status = web_view->CallFunction(
          std::string(), kGetWindowNameScript, args, &result);
      if (status.IsError())
        return status;
      std::string window_name;
      if (!result->GetAsString(&window_name))
        return Status(kUnknownError, "failed to get window name");
      if (window_name == name) {
        web_view_id = *it;
        found = true;
        break;
      }
    }
  }

  if (!found)
    return Status(kNoSuchWindow);

  if (session->overridden_geoposition ||
      session->overridden_network_conditions ||
      session->headless_download_directory ||
      session->chrome->IsMobileEmulationEnabled()) {
    // Connect to new window to apply session configuration
    WebView* web_view;
    Status status = session->chrome->GetWebViewById(web_view_id, &web_view);
    if (status.IsError())
      return status;
    status = web_view->ConnectIfNecessary();
    if (status.IsError())
      return status;

    // apply type specific configurations:
    if (session->overridden_geoposition) {
      status = web_view->OverrideGeolocation(*session->overridden_geoposition);
      if (status.IsError())
        return status;
    }
    if (session->overridden_network_conditions) {
      status = web_view->OverrideNetworkConditions(
          *session->overridden_network_conditions);
      if (status.IsError())
        return status;
    }
    if (session->headless_download_directory) {
      status = web_view->OverrideDownloadDirectoryIfNeeded(
          *session->headless_download_directory);
      if (status.IsError())
        return status;
    }
  }

  status = session->chrome->ActivateWebView(web_view_id);
  if (status.IsError())
    return status;
  session->window = web_view_id;
  session->SwitchToTopFrame();
  session->mouse_position = WebPoint(0, 0);
  return Status(kOk);
}

// Handles legacy format SetTimeout command.
// TODO(crbug.com/chromedriver/2596): Remove when we stop supporting legacy
// protocol.
Status ExecuteSetTimeoutLegacy(Session* session,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value) {
  double ms_double;
  if (!params.GetDouble("ms", &ms_double))
    return Status(kInvalidArgument, "'ms' must be a double");
  std::string type;
  if (!params.GetString("type", &type))
    return Status(kInvalidArgument, "'type' must be a string");

  base::TimeDelta timeout =
      base::TimeDelta::FromMilliseconds(static_cast<int>(ms_double));
  if (type == "implicit") {
    session->implicit_wait = timeout;
  } else if (type == "script") {
    session->script_timeout = timeout;
  } else if (type == "page load") {
    session->page_load_timeout =
        ((timeout < base::TimeDelta()) ? Session::kDefaultPageLoadTimeout
                                       : timeout);
  } else {
    return Status(kInvalidArgument, "unknown type of timeout:" + type);
  }
  return Status(kOk);
}

Status ExecuteSetTimeoutsW3C(Session* session,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value) {
  for (const auto& setting : params.DictItems()) {
    int64_t timeout_ms_int64 = -1;
    base::TimeDelta timeout;
    const std::string& type = setting.first;
    if (setting.second.is_none()) {
      if (type == "script")
        timeout = base::TimeDelta::Max();
      else
        return Status(kInvalidArgument, "timeout can not be null");
    } else {
        if (!GetOptionalSafeInt(&params, setting.first, &timeout_ms_int64)
            || timeout_ms_int64 < 0)
            return Status(kInvalidArgument,
                          "value must be a non-negative integer");
        else
            timeout = base::TimeDelta::FromMilliseconds(timeout_ms_int64);
    }
    if (type == "script") {
      session->script_timeout = timeout;
    } else if (type == "pageLoad") {
      session->page_load_timeout = timeout;
    } else if (type == "implicit") {
      session->implicit_wait = timeout;
    }
  }
  return Status(kOk);
}

Status ExecuteSetTimeouts(Session* session,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value) {
  // TODO(crbug.com/chromedriver/2596): Remove legacy version support when we
  // stop supporting non-W3C protocol. At that time, we can delete the legacy
  // function and merge the W3C function into this function.
  if (params.HasKey("ms")) {
    return ExecuteSetTimeoutLegacy(session, params, value);
  } else {
    return ExecuteSetTimeoutsW3C(session, params, value);
  }
}

Status ExecuteGetTimeouts(Session* session,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value) {
  base::DictionaryValue timeouts;
  if (session->script_timeout == base::TimeDelta::Max())
    timeouts.SetKey("script", base::Value());
  else
    SetSafeInt(&timeouts, "script", session->script_timeout.InMilliseconds());

  SetSafeInt(&timeouts, "pageLoad",
                        session->page_load_timeout.InMilliseconds());
  SetSafeInt(&timeouts, "implicit", session->implicit_wait.InMilliseconds());

  value->reset(timeouts.DeepCopy());
  return Status(kOk);
}

Status ExecuteSetScriptTimeout(Session* session,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value) {
  double ms;
  if (!params.GetDouble("ms", &ms) || ms < 0)
    return Status(kInvalidArgument, "'ms' must be a non-negative number");
  session->script_timeout =
      base::TimeDelta::FromMilliseconds(static_cast<int>(ms));
  return Status(kOk);
}

Status ExecuteImplicitlyWait(Session* session,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value) {
  double ms;
  if (!params.GetDouble("ms", &ms) || ms < 0)
    return Status(kInvalidArgument, "'ms' must be a non-negative number");
  session->implicit_wait =
      base::TimeDelta::FromMilliseconds(static_cast<int>(ms));
  return Status(kOk);
}

Status ExecuteIsLoading(Session* session,
                        const base::DictionaryValue& params,
                        std::unique_ptr<base::Value>* value) {
  WebView* web_view = NULL;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  status = web_view->ConnectIfNecessary();
  if (status.IsError())
    return status;

  bool is_pending;
  status = web_view->IsPendingNavigation(nullptr, &is_pending);
  if (status.IsError())
    return status;
  value->reset(new base::Value(is_pending));
  return Status(kOk);
}

Status ExecuteGetLocation(Session* session,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value) {
  if (!session->overridden_geoposition) {
    return Status(kUnknownError,
                  "Location must be set before it can be retrieved");
  }
  base::DictionaryValue location;
  location.SetDouble("latitude", session->overridden_geoposition->latitude);
  location.SetDouble("longitude", session->overridden_geoposition->longitude);
  location.SetDouble("accuracy", session->overridden_geoposition->accuracy);
  // Set a dummy altitude to make WebDriver clients happy.
  // https://code.google.com/p/chromedriver/issues/detail?id=281
  location.SetDouble("altitude", 0);
  value->reset(location.DeepCopy());
  return Status(kOk);
}

Status ExecuteGetNetworkConnection(Session* session,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value) {
  ChromeDesktopImpl* desktop = nullptr;
  Status status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsError())
    return status;
  if (!desktop->IsNetworkConnectionEnabled())
    return Status(kUnknownError, "network connection must be enabled");

  int connection_type = 0;
  connection_type = desktop->GetNetworkConnection();

  value->reset(new base::Value(connection_type));
  return Status(kOk);
}

Status ExecuteGetNetworkConditions(Session* session,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value) {
  if (!session->overridden_network_conditions) {
    return Status(kUnknownError,
                  "network conditions must be set before it can be retrieved");
  }
  base::DictionaryValue conditions;
  conditions.SetBoolean("offline",
                        session->overridden_network_conditions->offline);
  conditions.SetInteger("latency",
                        session->overridden_network_conditions->latency);
  conditions.SetInteger(
      "download_throughput",
      session->overridden_network_conditions->download_throughput);
  conditions.SetInteger(
      "upload_throughput",
      session->overridden_network_conditions->upload_throughput);
  value->reset(conditions.DeepCopy());
  return Status(kOk);
}

Status ExecuteSetNetworkConnection(Session* session,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value) {
  ChromeDesktopImpl* desktop = nullptr;
  Status status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsError())
    return status;
  if (!desktop->IsNetworkConnectionEnabled())
    return Status(kUnknownError, "network connection must be enabled");

  int connection_type;
  if (!params.GetInteger("parameters.type", &connection_type))
    return Status(kInvalidArgument, "invalid connection_type");

  desktop->SetNetworkConnection(connection_type);

  std::unique_ptr<NetworkConditions> network_conditions(
      new NetworkConditions());

  if (connection_type & kWifiMask) {
    network_conditions->latency = kWifiLatency;
    network_conditions->upload_throughput = kWifiThroughput;
    network_conditions->download_throughput = kWifiThroughput;
    network_conditions->offline = false;
  } else if (connection_type & k4GMask) {
    network_conditions->latency = k4GLatency;
    network_conditions->upload_throughput = k4GThroughput;
    network_conditions->download_throughput = k4GThroughput;
    network_conditions->offline = false;
  } else if (connection_type & k3GMask) {
    network_conditions->latency = k3GLatency;
    network_conditions->upload_throughput = k3GThroughput;
    network_conditions->download_throughput = k3GThroughput;
    network_conditions->offline = false;
  } else if (connection_type & k2GMask) {
    network_conditions->latency = k2GLatency;
    network_conditions->upload_throughput = k2GThroughput;
    network_conditions->download_throughput = k2GThroughput;
    network_conditions->offline = false;
  } else {
    network_conditions->latency = kAirplaneModeLatency;
    network_conditions->upload_throughput = kAirplaneModeThroughput;
    network_conditions->download_throughput = kAirplaneModeThroughput;
    network_conditions->offline = true;
  }

  session->overridden_network_conditions.reset(
      network_conditions.release());

  // Applies overridden network connection to all WebViews of the session
  // to ensure that network emulation is applied on a per-session basis
  // rather than the just to the current WebView.
  std::list<std::string> web_view_ids;
  status = session->chrome->GetWebViewIds(&web_view_ids,
                                          session->w3c_compliant);
  if (status.IsError())
    return status;

  for (std::string web_view_id : web_view_ids) {
    WebView* web_view;
    status = session->chrome->GetWebViewById(web_view_id, &web_view);
    if (status.IsError())
      return status;
    web_view->OverrideNetworkConditions(
      *session->overridden_network_conditions);
  }

  value->reset(new base::Value(connection_type));
  return Status(kOk);
}

Status ExecuteGetWindowPosition(Session* session,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value) {
  Chrome::WindowRect windowRect;
  Status status = session->chrome->GetWindowRect(session->window, &windowRect);

  if (status.IsError())
    return status;

  base::DictionaryValue position;
  position.SetInteger("x", windowRect.x);
  position.SetInteger("y", windowRect.y);
  value->reset(position.DeepCopy());
  return Status(kOk);
}

Status ExecuteSetWindowPosition(Session* session,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value) {
  double x = 0;
  double y = 0;
  if (!params.GetDouble("x", &x) || !params.GetDouble("y", &y))
    return Status(kInvalidArgument, "missing or invalid 'x' or 'y'");

  base::DictionaryValue rect_params;
  rect_params.SetInteger("x", static_cast<int>(x));
  rect_params.SetInteger("y", static_cast<int>(y));
  return session->chrome->SetWindowRect(session->window, rect_params);
}

Status ExecuteGetWindowSize(Session* session,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value) {
  Chrome::WindowRect windowRect;
  Status status = session->chrome->GetWindowRect(session->window, &windowRect);

  if (status.IsError())
    return status;

  base::DictionaryValue size;
  size.SetInteger("width", windowRect.width);
  size.SetInteger("height", windowRect.height);
  value->reset(size.DeepCopy());
  return Status(kOk);
}

Status ExecuteSetWindowSize(Session* session,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value) {
  double width = 0;
  double height = 0;
  if (!params.GetDouble("width", &width) ||
      !params.GetDouble("height", &height))
    return Status(kInvalidArgument, "missing or invalid 'width' or 'height'");

  base::DictionaryValue rect_params;
  rect_params.SetInteger("width", static_cast<int>(width));
  rect_params.SetInteger("height", static_cast<int>(height));
  return session->chrome->SetWindowRect(session->window, rect_params);
}

Status ExecuteGetAvailableLogTypes(Session* session,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value) {
  std::unique_ptr<base::ListValue> types(new base::ListValue());
  std::vector<WebDriverLog*> logs = session->GetAllLogs();
  for (std::vector<WebDriverLog*>::const_iterator log = logs.begin();
       log != logs.end();
       ++log) {
    types->AppendString((*log)->type());
  }
  *value = std::move(types);
  return Status(kOk);
}

Status ExecuteGetLog(Session* session,
                     const base::DictionaryValue& params,
                     std::unique_ptr<base::Value>* value) {
  std::string log_type;
  if (!params.GetString("type", &log_type)) {
    return Status(kInvalidArgument, "missing or invalid 'type'");
  }

  // Evaluate a JavaScript in the renderer process for the current tab, to flush
  // out any pending logging-related events.
  Status status = EvaluateScriptAndIgnoreResult(session, "1");
  if (status.IsError()) {
    // Sometimes a WebDriver client fetches logs to diagnose an error that has
    // occurred. It's possible that in the case of an error, the renderer is no
    // be longer available, but we should return the logs anyway. So log (but
    // don't fail on) any error that we get while evaluating the script.
    LOG(WARNING) << "Unable to evaluate script: " << status.message();
  }

  std::vector<WebDriverLog*> logs = session->GetAllLogs();
  for (std::vector<WebDriverLog*>::const_iterator log = logs.begin();
       log != logs.end();
       ++log) {
    if (log_type == (*log)->type()) {
      *value = (*log)->GetAndClearEntries();
      return Status(kOk);
    }
  }
  return Status(kInvalidArgument, "log type '" + log_type + "' not found");
}

Status ExecuteUploadFile(Session* session,
                         const base::DictionaryValue& params,
                         std::unique_ptr<base::Value>* value) {
  std::string base64_zip_data;
  if (!params.GetString("file", &base64_zip_data))
    return Status(kInvalidArgument, "missing or invalid 'file'");
  std::string zip_data;
  if (!Base64Decode(base64_zip_data, &zip_data))
    return Status(kUnknownError, "unable to decode 'file'");

  if (!session->temp_dir.IsValid()) {
    if (!session->temp_dir.CreateUniqueTempDir())
      return Status(kUnknownError, "unable to create temp dir");
  }
  base::FilePath upload_dir;
  if (!base::CreateTemporaryDirInDir(session->temp_dir.GetPath(),
                                     FILE_PATH_LITERAL("upload"),
                                     &upload_dir)) {
    return Status(kUnknownError, "unable to create temp dir");
  }
  std::string error_msg;
  base::FilePath upload;
  Status status = UnzipSoleFile(upload_dir, zip_data, &upload);
  if (status.IsError())
    return Status(kUnknownError, "unable to unzip 'file'", status);

  value->reset(new base::Value(upload.value()));
  return Status(kOk);
}

Status ExecuteUnimplementedCommand(Session* session,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value) {
  return Status(kUnknownCommand);
}

Status ExecuteGenerateTestReport(Session* session,
                                 const base::DictionaryValue& params,
                                 std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  std::string message, group;
  if (!params.GetString("message", &message))
    return Status(kInvalidArgument, "missing parameter 'message'");
  if (!params.GetString("group", &group))
    group = "default";

  base::DictionaryValue body;
  body.SetString("message", message);
  body.SetString("group", group);

  web_view->SendCommandAndGetResult("Page.generateTestReport", body, value);
  return Status(kOk);
}
