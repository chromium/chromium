// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session_commands.h"

#include <list>
#include <memory>
#include <thread>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"  // For CHECK macros.
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/basic_types.h"
#include "chrome/test/chromedriver/bidimapper/bidimapper.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/bidi_tracker.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/chrome_android_impl.h"
#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"
#include "chrome/test/chromedriver/chrome/chrome_impl.h"
#include "chrome/test/chromedriver/chrome/device_manager.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
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
  if (!web_view->IsServiceWorker() &&
      web_view->GetJavaScriptDialogManager()->IsDialogOpen()) {
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
  const base::DictionaryValue* options_dict;

  const base::Value* caps_dict =
      params.FindDictPath("capabilities.alwaysMatch");
  if (caps_dict &&
      GetChromeOptionsDictionary(base::Value::AsDictionaryValue(*caps_dict),
                                 &options_dict)) {
    absl::optional<bool> w3c = options_dict->FindBoolKey("w3c");
    if (w3c.has_value())
      return *w3c;
  }

  const base::Value* list = params.FindListPath("capabilities.firstMatch");
  if (list && list->GetList().size()) {
    const base::Value& caps_dict_ref = std::move(list->GetList()[0]);
    if (caps_dict_ref.is_dict() &&
        GetChromeOptionsDictionary(
            base::Value::AsDictionaryValue(caps_dict_ref), &options_dict)) {
      absl::optional<bool> w3c = options_dict->FindBoolKey("w3c");
      if (w3c.has_value())
        return *w3c;
    }
  }

  caps_dict = params.FindDictKey("desiredCapabilities");
  if (caps_dict &&
      GetChromeOptionsDictionary(base::Value::AsDictionaryValue(*caps_dict),
                                 &options_dict)) {
    absl::optional<bool> w3c = options_dict->FindBoolKey("w3c");
    if (w3c.has_value())
      return *w3c;
  }

  if (!params.FindKey("capabilities") &&
      params.FindKey("desiredCapabilities")) {
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
  caps->SetStringKey("browserName", base::ToLowerASCII(kBrowserShortName));
  caps->SetStringKey(session->w3c_compliant ? "browserVersion" : "version",
                     session->chrome->GetBrowserInfo()->browser_version);
  std::string operatingSystemName = session->chrome->GetOperatingSystemName();
  if (operatingSystemName.find("Windows") != std::string::npos)
    operatingSystemName = "Windows";
  if (session->w3c_compliant) {
    caps->SetStringKey("platformName", base::ToLowerASCII(operatingSystemName));
  } else {
    caps->SetStringKey("platform", operatingSystemName);
  }
  caps->SetStringKey("pageLoadStrategy", session->chrome->page_load_strategy());
  caps->SetBoolKey("acceptInsecureCerts", capabilities.accept_insecure_certs);
  const base::Value* proxy = desired_caps.FindKey("proxy");
  if (proxy == nullptr || proxy->is_none())
    caps->SetKey("proxy", base::Value(base::Value::Type::DICTIONARY));
  else
    caps->SetKey("proxy", proxy->Clone());
  // add setWindowRect based on whether we are desktop/android/remote
  if (capabilities.IsAndroid() || capabilities.IsRemoteBrowser()) {
    caps->SetBoolKey("setWindowRect", false);
  } else {
    caps->SetBoolKey("setWindowRect", true);
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
  caps->SetBoolKey("strictFileInteractability",
                   session->strict_file_interactability);
  caps->SetStringKey(session->w3c_compliant ? "unhandledPromptBehavior"
                                            : "unexpectedAlertBehaviour",
                     session->unhandled_prompt_behavior);

  // Extensions defined by the W3C.
  // See https://w3c.github.io/webauthn/#sctn-automation-webdriver-capability
  caps->SetBoolKey("webauthn:virtualAuthenticators", !capabilities.IsAndroid());
  caps->SetBoolKey("webauthn:extension:largeBlob", !capabilities.IsAndroid());
  caps->SetBoolKey("webauthn:extension:credBlob", !capabilities.IsAndroid());

  // Chrome-specific extensions.
  const std::string chromedriverVersionKey = base::StringPrintf(
      "%s.%sVersion", base::ToLowerASCII(kBrowserShortName).c_str(),
      base::ToLowerASCII(kChromeDriverProductShortName).c_str());
  caps->SetStringPath(chromedriverVersionKey, kChromeDriverVersion);
  const std::string debuggerAddressKey =
      base::StringPrintf("%s.debuggerAddress", kChromeDriverOptionsKeyPrefixed);
  caps->SetStringPath(debuggerAddressKey, session->chrome->GetBrowserInfo()
                                              ->debugger_endpoint.Address()
                                              .ToString());
  ChromeDesktopImpl* desktop = nullptr;
  Status status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsOk()) {
    const std::string userDataDirKey = base::StringPrintf(
        "%s.userDataDir", base::ToLowerASCII(kBrowserShortName).c_str());
    caps->SetStringPath(
        userDataDirKey,
        desktop->command().GetSwitchValuePath("user-data-dir").AsUTF8Unsafe());
    caps->SetBoolKey("networkConnectionEnabled",
                     desktop->IsNetworkConnectionEnabled());
  }

  // Legacy capabilities.
  if (!session->w3c_compliant) {
    caps->SetBoolKey("javascriptEnabled", true);
    caps->SetBoolKey("takesScreenshot", true);
    caps->SetBoolKey("takesHeapSnapshot", true);
    caps->SetBoolKey("handlesAlerts", true);
    caps->SetBoolKey("databaseEnabled", false);
    caps->SetBoolKey("locationContextEnabled", true);
    caps->SetBoolKey("mobileEmulationEnabled",
                     session->chrome->IsMobileEmulationEnabled());
    caps->SetBoolKey("browserConnectionEnabled", false);
    caps->SetBoolKey("cssSelectorsEnabled", true);
    caps->SetBoolKey("webStorageEnabled", true);
    caps->SetBoolKey("rotatable", false);
    caps->SetBoolKey("acceptSslCerts", capabilities.accept_insecure_certs);
    caps->SetBoolKey("nativeEvents", true);
    caps->SetBoolKey("hasTouchScreen", session->chrome->HasTouchScreen());
  }

  if (session->webSocketUrl) {
    caps->SetStringKey("webSocketUrl",
                       "ws://" + session->host + "/session/" + session->id);
  }

  return caps;
}

Status CheckSessionCreated(Session* session) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return Status(kSessionNotCreated, status);

  status = web_view->ConnectIfNecessary();
  if (status.IsError())
    return Status(kSessionNotCreated, status);

  base::Value::List args;
  std::unique_ptr<base::Value> result(new base::Value(0));
  status = web_view->CallFunction(session->GetCurrentFrameId(),
                                  "function(s) { return 1; }", args, &result);
  if (status.IsError())
    return Status(kSessionNotCreated, status);

  if (!result->is_int() || result->GetInt() != 1) {
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

  if (session->webSocketUrl) {
    BidiTracker* bidi_tracker = new BidiTracker();
    bidi_tracker->SetBidiCallback(base::BindRepeating(
        &Session::OnBidiResponse, base::Unretained(session)));
    devtools_event_listeners.emplace_back(bidi_tracker);
  }

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
    base::Value body(base::Value::Type::DICTIONARY);
    body.SetKey("capabilities", session->capabilities->Clone());
    body.SetStringKey("sessionId", session->id);
    *value = base::Value::ToUniquePtrValue(body.Clone());
  } else {
    *value = base::Value::ToUniquePtrValue(session->capabilities->Clone());
  }

  status = CheckSessionCreated(session);
  if (status.IsError())
    return status;

  if (session->webSocketUrl) {
    WebView* web_view = nullptr;
    status = session->GetTargetWindow(&web_view);
    if (status.IsError())
      return status;
    session->bidi_mapper_web_view_id = session->window;
    ChromeImpl* chrome = static_cast<ChromeImpl*>(session->chrome.get());
    DevToolsClient* client = chrome->Client();

    {
      base::Value body(base::Value::Type::DICTIONARY);
      body.SetStringKey("bindingName", "cdp");
      body.SetStringKey("targetId", session->window);
      client->SendCommandAndIgnoreResponse(
          "Target.exposeDevToolsProtocol",
          base::Value::AsDictionaryValue(body));
    }

    {
      std::unique_ptr<base::Value> result;
      base::Value body(base::Value::Type::DICTIONARY);
      body.SetStringKey("name", "sendBidiResponse");
      web_view->SendCommandAndGetResult(
          "Runtime.addBinding", base::Value::AsDictionaryValue(body), &result);
    }

    status = EvaluateScriptAndIgnoreResult(session, kMapperScript);
    if (status.IsError())
      return status;

    {
      std::unique_ptr<base::Value> result;
      base::Value body(base::Value::Type::DICTIONARY);
      std::string window_id;
      if (!base::JSONWriter::Write(session->window, &window_id)) {
        return Status(kUnknownError,
                      "cannot serialize be window id: " + session->window);
      }
      body.SetStringKey("expression",
                        "window.setSelfTargetId(" + window_id + ")");
      status = web_view->SendCommandAndGetResult(
          "Runtime.evaluate", base::Value::AsDictionaryValue(body), &result);
      if (status.IsError())
        return status;
    }

    base::RepeatingCallback<Status(bool*)> bidi_mapper_is_launched =
        base::BindRepeating(
            [](Session* session, bool* condition_is_met) {
              *condition_is_met = session->BidiMapperIsLaunched();
              return Status{kOk};
            },
            base::Unretained(session));
    // Assume that BiDiMapper initialization requires the same time as a regular
    // script
    web_view->HandleEventsUntil(bidi_mapper_is_launched,
                                Timeout(session->script_timeout));

    {
      // Create a new tab because the default one is occupied by the BiDiMapper
      std::string web_view_id;
      status = session->chrome->NewWindow(
          session->window, Chrome::WindowType::kTab, &web_view_id);

      if (status.IsError())
        return status;

      std::string handle = WebViewIdToWindowHandle(web_view_id);

      std::unique_ptr<base::Value> result;
      base::Value body(base::Value::Type::DICTIONARY);
      body.GetDict().Set("handle", handle);

      status = ExecuteSwitchToWindow(
          session, base::Value::AsDictionaryValue(body), &result);
    }
  }

  return status;
}

}  // namespace

namespace internal {

Status ConfigureSession(Session* session,
                        const base::DictionaryValue& params,
                        const base::DictionaryValue** desired_caps,
                        base::DictionaryValue* merged_caps,
                        Capabilities* capabilities) {
  session->driver_log =
      std::make_unique<WebDriverLog>(WebDriverLog::kDriverType, Log::kAll);

  session->w3c_compliant = GetW3CSetting(params);
  if (session->w3c_compliant) {
    Status status = ProcessCapabilities(params, merged_caps);
    if (status.IsError())
      return status;
    *desired_caps = merged_caps;
  } else {
    const base::Value* caps = params.FindDictKey("desiredCapabilities");
    if (!caps)
      return Status(kSessionNotCreated, "Missing or invalid capabilities");

    *desired_caps = static_cast<const base::DictionaryValue*>(caps);
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
  if (!session->chrome->GetBrowserInfo()->is_headless)
    return Status(kOk);

  const std::string* download_directory = nullptr;
  if (capabilities.prefs) {
    download_directory =
        capabilities.prefs->FindStringPath("download.default_directory");
    if (!download_directory) {
      download_directory =
          capabilities.prefs->FindStringKey("download.default_directory");
    }
  }
  session->headless_download_directory = std::make_unique<std::string>(
      download_directory ? *download_directory : ".");

  WebView* first_view;
  session->chrome->GetWebViewById(session->window, &first_view);
  return first_view->OverrideDownloadDirectoryIfNeeded(
      *session->headless_download_directory);
}

}  // namespace internal

bool MergeCapabilities(const base::DictionaryValue* always_match,
                       const base::DictionaryValue* first_match,
                       base::DictionaryValue* merged) {
  CHECK(always_match);
  CHECK(first_match);
  CHECK(merged);
  merged->DictClear();

  for (auto kv : first_match->DictItems()) {
    if (always_match->FindKey(kv.first)) {
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
// Currently, we only check "browserName", "platformName", and webauthn
// capabilities but more can be added as necessary.
bool MatchCapabilities(const base::DictionaryValue* capabilities) {
  const base::Value* name = capabilities->FindKey("browserName");
  if (name && !name->is_none()) {
    if (!(name->is_string() && name->GetString() == kBrowserCapabilityName))
      return false;
  }

  const base::DictionaryValue* chrome_options;
  const bool has_chrome_options =
      GetChromeOptionsDictionary(*capabilities, &chrome_options);

  bool is_android = has_chrome_options &&
                    chrome_options->FindStringKey("androidPackage") != nullptr;

  const base::Value* platform_name_value =
      capabilities->FindPath("platformName");
  if (platform_name_value && !platform_name_value->is_none()) {
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

  const base::Value* virtual_authenticators_value =
      capabilities->FindPath("webauthn:virtualAuthenticators");
  if (virtual_authenticators_value) {
    if (!virtual_authenticators_value->is_bool() ||
        (virtual_authenticators_value->GetBool() && is_android)) {
      return false;
    }
  }

  const base::Value* large_blob_value =
      capabilities->FindPath("webauthn:extension:largeBlob");
  if (large_blob_value) {
    if (!large_blob_value->is_bool() ||
        (large_blob_value->GetBool() && is_android)) {
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
  const base::Value* capabilities_request = params.FindDictKey("capabilities");
  if (!capabilities_request)
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
  base::Value default_list(base::Value::Type::LIST);
  const base::Value* all_first_match_capabilities =
      capabilities_request->FindKey("firstMatch");
  if (all_first_match_capabilities == nullptr) {
    default_list.Append(base::Value(base::Value::Type::DICTIONARY));
    all_first_match_capabilities = &default_list;
  } else if (all_first_match_capabilities->is_list()) {
    if (all_first_match_capabilities->GetList().size() < 1)
      return Status(kInvalidArgument,
                    "'firstMatch' must contain at least one entry");
  } else {
    return Status(kInvalidArgument, "'firstMatch' must be a JSON list");
  }

  // 4. Let validated first match capabilities be an empty JSON List.
  std::vector<const base::DictionaryValue*> validated_first_match_capabilities;

  // 5. Validate all first match capabilities.
  for (size_t i = 0; i < all_first_match_capabilities->GetList().size(); ++i) {
    const base::Value& first_match = all_first_match_capabilities->GetList()[i];
    if (!first_match.is_dict()) {
      return Status(kInvalidArgument,
                    base::StringPrintf(
                        "entry %zu of 'firstMatch' must be a JSON object", i));
    }
    Capabilities cap;
    Status status = cap.Parse(base::Value::AsDictionaryValue(first_match));
    if (status.IsError())
      return Status(
          kInvalidArgument,
          base::StringPrintf("entry %zu of 'firstMatch' is invalid", i),
          status);
    validated_first_match_capabilities.push_back(
        &base::Value::AsDictionaryValue(first_match));
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
    if (session->chrome != nullptr)
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
  *value = base::Value::ToUniquePtrValue(session->capabilities->Clone());
  return Status(kOk);
}

Status ExecuteGetCurrentWindowHandle(Session* session,
                                     const base::DictionaryValue& params,
                                     std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;
  status = web_view->ConnectIfNecessary();
  if (status.IsError())
    return status;
  *value =
      std::make_unique<base::Value>(WebViewIdToWindowHandle(web_view->GetId()));
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
  if (session->webSocketUrl) {
    is_last_web_view = web_view_ids.size() <= 2u;
  }
  web_view_ids.clear();

  WebView* web_view = nullptr;
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

  status = session->chrome->CloseWebView(web_view->GetId());
  if (status.IsError())
    return status;

  if (is_last_web_view) {
    // If there is only one window left, call quit as well.
    session->quit = true;
    status = session->chrome->Quit();
    if (status.IsOk())
      *value = std::make_unique<base::Value>(base::Value::Type::LIST);
  } else {
    status = ExecuteGetWindowHandles(session, base::DictionaryValue(), value);
    if (status.IsError())
      return status;
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

  if (session->webSocketUrl) {
    std::string mapper_view_id;
    // TODO(chromedriver:4181): How do we know for sure that the first page is
    // the mapper?
    status = session->chrome->GetWebViewIdForFirstTab(&mapper_view_id,
                                                      session->w3c_compliant);
    auto it =
        std::find(web_view_ids.begin(), web_view_ids.end(), mapper_view_id);
    if (it != web_view_ids.end()) {
      web_view_ids.erase(it);
    }
  }

  std::unique_ptr<base::Value> window_ids(
      new base::Value(base::Value::Type::LIST));
  for (std::list<std::string>::const_iterator it = web_view_ids.begin();
       it != web_view_ids.end(); ++it) {
    window_ids->Append(WebViewIdToWindowHandle(*it));
  }
  *value = std::move(window_ids);
  return Status(kOk);
}

Status ExecuteSwitchToWindow(Session* session,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value) {
  const std::string* name;
  if (session->w3c_compliant) {
    name = params.FindStringKey("handle");
    if (!name)
      return Status(kInvalidArgument, "'handle' must be a string");
  } else {
    name = params.FindStringKey("name");
    if (!name)
      return Status(kInvalidArgument, "'name' must be a string");
  }

  std::list<std::string> web_view_ids;
  Status status = session->chrome->GetWebViewIds(&web_view_ids,
                                                 session->w3c_compliant);
  if (status.IsError())
    return status;

  std::string web_view_id;
  bool found = false;
  if (WindowHandleToWebViewId(*name, &web_view_id)) {
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
    base::Value::List args;
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
      if (!result->is_string())
        return Status(kUnknownError, "failed to get window name");
      if (result->GetString() == *name) {
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
    status = session->chrome->GetWebViewById(web_view_id, &web_view);
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
  absl::optional<double> maybe_ms = params.FindDoubleKey("ms");
  if (!maybe_ms.has_value())
    return Status(kInvalidArgument, "'ms' must be a double");

  const std::string* type = params.FindStringKey("type");
  if (!type)
    return Status(kInvalidArgument, "'type' must be a string");

  base::TimeDelta timeout =
      base::Milliseconds(static_cast<int>(maybe_ms.value()));
  if (*type == "implicit") {
    session->implicit_wait = timeout;
  } else if (*type == "script") {
    session->script_timeout = timeout;
  } else if (*type == "page load") {
    session->page_load_timeout =
        ((timeout.is_negative()) ? Session::kDefaultPageLoadTimeout : timeout);
  } else {
    return Status(kInvalidArgument, "unknown type of timeout:" + *type);
  }
  return Status(kOk);
}

Status ExecuteSetTimeoutsW3C(Session* session,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value) {
  for (auto setting : params.DictItems()) {
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
          timeout = base::Milliseconds(timeout_ms_int64);
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
  if (params.FindKey("ms")) {
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

  *value = base::Value::ToUniquePtrValue(timeouts.Clone());
  return Status(kOk);
}

Status ExecuteSetScriptTimeout(Session* session,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value) {
  absl::optional<double> maybe_ms = params.FindDoubleKey("ms");
  if (!maybe_ms.has_value() || maybe_ms.value() < 0)
    return Status(kInvalidArgument, "'ms' must be a non-negative number");
  session->script_timeout =
      base::Milliseconds(static_cast<int>(maybe_ms.value()));
  return Status(kOk);
}

Status ExecuteImplicitlyWait(Session* session,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value) {
  absl::optional<double> maybe_ms = params.FindDoubleKey("ms");
  if (!maybe_ms.has_value() || maybe_ms.value() < 0)
    return Status(kInvalidArgument, "'ms' must be a non-negative number");
  session->implicit_wait =
      base::Milliseconds(static_cast<int>(maybe_ms.value()));
  return Status(kOk);
}

Status ExecuteIsLoading(Session* session,
                        const base::DictionaryValue& params,
                        std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
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
  *value = std::make_unique<base::Value>(is_pending);
  return Status(kOk);
}

Status ExecuteGetLocation(Session* session,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value) {
  if (!session->overridden_geoposition) {
    return Status(kUnknownError,
                  "Location must be set before it can be retrieved");
  }
  base::Value location(base::Value::Type::DICTIONARY);
  location.SetDoubleKey("latitude", session->overridden_geoposition->latitude);
  location.SetDoubleKey("longitude",
                        session->overridden_geoposition->longitude);
  location.SetDoubleKey("accuracy", session->overridden_geoposition->accuracy);
  // Set a dummy altitude to make WebDriver clients happy.
  // https://code.google.com/p/chromedriver/issues/detail?id=281
  location.SetDoubleKey("altitude", 0);
  *value = base::Value::ToUniquePtrValue(location.Clone());
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

  *value = std::make_unique<base::Value>(connection_type);
  return Status(kOk);
}

Status ExecuteGetNetworkConditions(Session* session,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value) {
  if (!session->overridden_network_conditions) {
    return Status(kUnknownError,
                  "network conditions must be set before it can be retrieved");
  }
  base::Value conditions(base::Value::Type::DICTIONARY);
  conditions.SetBoolKey("offline",
                        session->overridden_network_conditions->offline);
  conditions.SetIntKey("latency",
                       session->overridden_network_conditions->latency);
  conditions.SetIntKey(
      "download_throughput",
      session->overridden_network_conditions->download_throughput);
  conditions.SetIntKey(
      "upload_throughput",
      session->overridden_network_conditions->upload_throughput);
  *value = base::Value::ToUniquePtrValue(conditions.Clone());
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

  absl::optional<int> connection_type = params.FindIntPath("parameters.type");
  if (!connection_type)
    return Status(kInvalidArgument, "invalid connection_type");

  desktop->SetNetworkConnection(*connection_type);

  std::unique_ptr<NetworkConditions> network_conditions(
      new NetworkConditions());

  if (*connection_type & kWifiMask) {
    network_conditions->latency = kWifiLatency;
    network_conditions->upload_throughput = kWifiThroughput;
    network_conditions->download_throughput = kWifiThroughput;
    network_conditions->offline = false;
  } else if (*connection_type & k4GMask) {
    network_conditions->latency = k4GLatency;
    network_conditions->upload_throughput = k4GThroughput;
    network_conditions->download_throughput = k4GThroughput;
    network_conditions->offline = false;
  } else if (*connection_type & k3GMask) {
    network_conditions->latency = k3GLatency;
    network_conditions->upload_throughput = k3GThroughput;
    network_conditions->download_throughput = k3GThroughput;
    network_conditions->offline = false;
  } else if (*connection_type & k2GMask) {
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

  *value = std::make_unique<base::Value>(*connection_type);
  return Status(kOk);
}

Status ExecuteGetWindowPosition(Session* session,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value) {
  Chrome::WindowRect windowRect;
  Status status = session->chrome->GetWindowRect(session->window, &windowRect);

  if (status.IsError())
    return status;

  base::Value position(base::Value::Type::DICTIONARY);
  position.SetIntKey("x", windowRect.x);
  position.SetIntKey("y", windowRect.y);
  *value = base::Value::ToUniquePtrValue(position.Clone());
  return Status(kOk);
}

Status ExecuteSetWindowPosition(Session* session,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value) {
  absl::optional<double> maybe_x = params.FindDoubleKey("x");
  absl::optional<double> maybe_y = params.FindDoubleKey("y");

  if (!maybe_x.has_value() || !maybe_y.has_value())
    return Status(kInvalidArgument, "missing or invalid 'x' or 'y'");

  base::Value rect_params(base::Value::Type::DICTIONARY);
  rect_params.SetIntKey("x", static_cast<int>(maybe_x.value()));
  rect_params.SetIntKey("y", static_cast<int>(maybe_y.value()));
  return session->chrome->SetWindowRect(
      session->window, base::Value::AsDictionaryValue(rect_params));
}

Status ExecuteGetWindowSize(Session* session,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value) {
  Chrome::WindowRect windowRect;
  Status status = session->chrome->GetWindowRect(session->window, &windowRect);

  if (status.IsError())
    return status;

  base::Value size(base::Value::Type::DICTIONARY);
  size.SetIntKey("width", windowRect.width);
  size.SetIntKey("height", windowRect.height);
  *value = base::Value::ToUniquePtrValue(size.Clone());
  return Status(kOk);
}

Status ExecuteSetWindowSize(Session* session,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value) {
  absl::optional<double> maybe_width = params.FindDoubleKey("width");
  absl::optional<double> maybe_height = params.FindDoubleKey("height");

  if (!maybe_width.has_value() || !maybe_height.has_value())
    return Status(kInvalidArgument, "missing or invalid 'width' or 'height'");

  base::Value rect_params(base::Value::Type::DICTIONARY);
  rect_params.SetIntKey("width", static_cast<int>(maybe_width.value()));
  rect_params.SetIntKey("height", static_cast<int>(maybe_height.value()));
  return session->chrome->SetWindowRect(
      session->window, base::Value::AsDictionaryValue(rect_params));
}

Status ExecuteGetAvailableLogTypes(Session* session,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value) {
  std::unique_ptr<base::Value::List> types(new base::Value::List());
  std::vector<WebDriverLog*> logs = session->GetAllLogs();
  for (std::vector<WebDriverLog*>::const_iterator log = logs.begin();
       log != logs.end();
       ++log) {
    types->Append((*log)->type());
  }
  *value = std::make_unique<base::Value>(base::Value(std::move(*types)));
  return Status(kOk);
}

Status ExecuteGetLog(Session* session,
                     const base::DictionaryValue& params,
                     std::unique_ptr<base::Value>* value) {
  const std::string* log_type = params.FindStringKey("type");
  if (!log_type) {
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
    if (*log_type == (*log)->type()) {
      *value = (*log)->GetAndClearEntries();
      return Status(kOk);
    }
  }
  return Status(kInvalidArgument, "log type '" + *log_type + "' not found");
}

Status ExecuteUploadFile(Session* session,
                         const base::DictionaryValue& params,
                         std::unique_ptr<base::Value>* value) {
  const std::string* base64_zip_data = params.FindStringKey("file");
  if (!base64_zip_data)
    return Status(kInvalidArgument, "missing or invalid 'file'");
  std::string zip_data;
  if (!Base64Decode(*base64_zip_data, &zip_data))
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

  *value = std::make_unique<base::Value>(upload.AsUTF8Unsafe());
  return Status(kOk);
}

Status ExecuteUnimplementedCommand(Session* session,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value) {
  return Status(kUnknownCommand);
}

Status ExecuteSetSPCTransactionMode(Session* session,
                                    const base::DictionaryValue& params,
                                    std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  const std::string* mode = params.FindStringKey("mode");
  if (!mode)
    return Status(kInvalidArgument, "missing parameter 'mode'");

  base::Value body(base::Value::Type::DICTIONARY);
  body.SetStringKey("mode", *mode);

  return web_view->SendCommandAndGetResult("Page.setSPCTransactionMode",
                                           base::Value::AsDictionaryValue(body),
                                           value);
}

Status ExecuteGenerateTestReport(Session* session,
                                 const base::DictionaryValue& params,
                                 std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  const std::string* message = params.FindStringKey("message");
  if (!message)
    return Status(kInvalidArgument, "missing parameter 'message'");
  const std::string* group = params.FindStringKey("group");

  base::Value body(base::Value::Type::DICTIONARY);
  body.SetStringKey("message", *message);
  body.SetStringKey("group", group ? *group : "default");

  web_view->SendCommandAndGetResult(
      "Page.generateTestReport", base::Value::AsDictionaryValue(body), value);
  return Status(kOk);
}

Status ExecuteSetTimeZone(Session* session,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  const std::string* time_zone = params.FindStringKey("time_zone");
  if (!time_zone)
    return Status(kInvalidArgument, "missing parameter 'time_zone'");

  base::Value body(base::Value::Type::DICTIONARY);
  body.SetStringKey("timezoneId", *time_zone);

  web_view->SendCommandAndGetResult("Emulation.setTimezoneOverride",
                                    base::Value::AsDictionaryValue(body),
                                    value);
  return Status(kOk);
}

// Run a BiDi command
Status ExecuteBidiCommand(Session* session,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value) {
  // session == nullptr is a valid case: ExecuteQuit has already been handled
  // in the session thread but the following
  // TerminateSessionThreadOnCommandThread has not yet been executed (the later
  // destroys the session thread) The connection has already been accepted by
  // the CMD thread but soon it will be closed. We don't need to do anything.
  if (session == nullptr) {
    return Status{kNoSuchFrame, "session not found"};
  }
  std::string data;
  params.GetString("bidiCommand", &data);

  WebView* web_view = nullptr;
  Status status = session->chrome->GetWebViewById(
      session->bidi_mapper_web_view_id, &web_view);
  if (status.IsError()) {
    return status;
  }

  absl::optional<base::Value> dataParsed =
      base::JSONReader::Read(data, base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  if (!dataParsed) {
    return Status(kUnknownError, "cannot parse the BiDi command: " + data);
  }

  if (!dataParsed->is_dict()) {
    return Status(kUnknownError,
                  "a JSON map is expected as a BiDi command: " + data);
  }

  absl::optional<int> cmd_id = dataParsed->GetDict().FindInt("id");
  if (!cmd_id) {
    return Status(kUnknownError, "BiDi command is missing 'id' field: " + data);
  }

  std::string* method = dataParsed->GetDict().FindString("method");
  if (!method) {
    return Status(kUnknownError,
                  "BiDi command is missing 'method' field: " + data);
  }

  std::string msg;
  if (!base::JSONWriter::Write(data, &msg)) {
    return Status(kUnknownError, "cannot serialize be BiDi command: " + data);
  }
  std::string expression = "onBidiMessage(" + msg + ")";

  if (*method == "browsingContext.close") {
    // Closing of the context is handled in a blocking way.
    // This simplifies us closing the browser if the last tab was closed.
    session->awaited_bidi_response_id = *cmd_id;
    status = web_view->EvaluateScript(std::string(), expression, false, value);
    base::RepeatingCallback<Status(bool*)> bidi_response_is_received =
        base::BindRepeating(
            [](Session* session, int cmd_id, bool* condition_is_met) {
              *condition_is_met = session->awaited_bidi_response_id != cmd_id;
              return Status{kOk};
            },
            base::Unretained(session), *cmd_id);
    if (status.IsError()) {
      return status;
    }

    // The timeout is the same as in ChromeImpl::CloseTarget
    status = web_view->HandleEventsUntil(std::move(bidi_response_is_received),
                                         Timeout(base::Seconds(20)));
    if (status.code() == kTimeout) {
      // It looks like something is going wrong with the BiDiMapper.
      // Terminating the session...
      session->quit = true;
      status = session->chrome->Quit();
      return Status(kUnknownError, "failed to close window in 20 seconds");
    }
    if (status.IsError())
      return status;

    std::list<std::string> web_view_ids;
    status =
        session->chrome->GetWebViewIds(&web_view_ids, session->w3c_compliant);
    if (status.IsError())
      return status;

    bool is_last_web_view = web_view_ids.size() <= 1u;
    if (is_last_web_view) {
      session->quit = true;
      status = session->chrome->Quit();
    }
  } else {
    status = web_view->EvaluateScript(std::string(), expression, false, value);
  }

  return status;
}
