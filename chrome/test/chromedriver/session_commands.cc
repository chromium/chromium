// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session_commands.h"

#include <list>
#include <memory>
#include <thread>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"  // For CHECK macros.
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/test/chromedriver/basic_types.h"
#include "chrome/test/chromedriver/bidimapper/bidimapper.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/bidi_tracker.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"
#include "chrome/test/chromedriver/chrome/chrome_impl.h"
#include "chrome/test/chromedriver/chrome/device_manager.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/geoposition.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/chrome_launcher.h"
#include "chrome/test/chromedriver/command_listener.h"
#include "chrome/test/chromedriver/constants/version.h"
#include "chrome/test/chromedriver/logging.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/util.h"
#include "services/device/public/cpp/generic_sensor/orientation_util.h"
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
                                     const bool await_promise = false) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;
  if (!web_view->IsServiceWorker() && web_view->IsDialogOpen()) {
    std::string alert_text;
    status = web_view->GetDialogMessage(alert_text);
    if (status.IsError())
      return Status(kUnexpectedAlertOpen);
    return Status(kUnexpectedAlertOpen, "{Alert text : " + alert_text + "}");
  }
  std::string frame_id = session->GetCurrentFrameId();
  std::unique_ptr<base::Value> result;
  return web_view->EvaluateScript(frame_id, expression, await_promise, &result);
}

base::RepeatingCallback<Status(bool*)> BidiResponseIsReceivedCallback(
    Session* session) {
  return base::BindRepeating(
      [](Session* session, bool* condition_is_met) {
        *condition_is_met = !session->awaiting_bidi_response;
        return Status{kOk};
      },
      base::Unretained(session));
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

InitSessionParams::~InitSessionParams() = default;

// Look for W3C mode setting in InitSession command parameters.
bool GetW3CSetting(const base::Value::Dict& params) {
  const base::Value::Dict* options_dict = nullptr;

  const base::Value::Dict* caps_dict =
      params.FindDictByDottedPath("capabilities.alwaysMatch");
  if (caps_dict && GetChromeOptionsDictionary(*caps_dict, &options_dict)) {
    std::optional<bool> w3c = options_dict->FindBool("w3c");
    if (w3c.has_value())
      return *w3c;
  }

  const base::Value::List* list =
      params.FindListByDottedPath("capabilities.firstMatch");
  if (list && list->size()) {
    const base::Value& caps_dict_ref = (*list)[0];
    if (caps_dict_ref.is_dict() &&
        GetChromeOptionsDictionary(caps_dict_ref.GetDict(), &options_dict)) {
      std::optional<bool> w3c = options_dict->FindBool("w3c");
      if (w3c.has_value())
        return *w3c;
    }
  }

  caps_dict = params.FindDict("desiredCapabilities");
  if (caps_dict && GetChromeOptionsDictionary(*caps_dict, &options_dict)) {
    std::optional<bool> w3c = options_dict->FindBool("w3c");
    if (w3c.has_value())
      return *w3c;
  }

  if (!params.contains("capabilities") &&
      params.contains("desiredCapabilities")) {
    return false;
  }

  return kW3CDefault;
}

namespace {

std::string PlatformNameToW3C(const std::string& platform_name) {
  std::string result = base::ToLowerASCII(platform_name);
  if (base::StartsWith(result, "mac")) {
    result = "mac";
  }
  return result;
}

// Creates a JSON object (represented by base::Value::Dict) that contains
// the capabilities, for returning to the client app as the result of New
// Session command.
base::Value::Dict CreateCapabilities(Session* session,
                                     const Capabilities& capabilities,
                                     const base::Value::Dict& desired_caps) {
  base::Value::Dict caps;

  // Capabilities defined by W3C. Some of these capabilities have different
  // names in legacy mode.
  caps.Set("browserName", session->chrome->GetBrowserInfo()->is_headless_shell
                              ? kHeadlessShellCapabilityName
                              : kBrowserCapabilityName);
  caps.Set(session->w3c_compliant ? "browserVersion" : "version",
           session->chrome->GetBrowserInfo()->browser_version);
  std::string os_name = session->chrome->GetOperatingSystemName();
  if (os_name.find("Windows") != std::string::npos)
    os_name = "Windows";
  if (session->w3c_compliant) {
    caps.Set("platformName", PlatformNameToW3C(os_name));
  } else {
    caps.Set("platform", os_name);
  }
  caps.Set("pageLoadStrategy", session->chrome->page_load_strategy());
  caps.Set("acceptInsecureCerts", capabilities.accept_insecure_certs);

  const base::Value* proxy = desired_caps.Find("proxy");
  if (proxy == nullptr || proxy->is_none())
    caps.Set("proxy", base::Value::Dict());
  else
    caps.Set("proxy", proxy->Clone());

  // add setWindowRect based on whether we are desktop/android/remote
  if (capabilities.IsAndroid() || capabilities.IsRemoteBrowser()) {
    caps.Set("setWindowRect", false);
  } else {
    caps.Set("setWindowRect", true);
  }
  if (session->script_timeout == base::TimeDelta::Max()) {
    caps.SetByDottedPath("timeouts.script", base::Value());
  } else {
    SetSafeInt(caps, "timeouts.script",
               session->script_timeout.InMilliseconds());
  }
  SetSafeInt(caps, "timeouts.pageLoad",
             session->page_load_timeout.InMilliseconds());
  SetSafeInt(caps, "timeouts.implicit",
             session->implicit_wait.InMilliseconds());
  caps.Set("strictFileInteractability", session->strict_file_interactability);
  caps.Set(session->w3c_compliant ? "unhandledPromptBehavior"
                                  : "unexpectedAlertBehaviour",
           session->unhandled_prompt_behavior.CapabilityView());

  // Extensions defined by the W3C.
  // See https://w3c.github.io/webauthn/#sctn-automation-webdriver-capability
  caps.Set("webauthn:virtualAuthenticators", !capabilities.IsAndroid());
  caps.Set("webauthn:extension:largeBlob", !capabilities.IsAndroid());
  caps.Set("webauthn:extension:minPinLength", !capabilities.IsAndroid());
  caps.Set("webauthn:extension:credBlob", !capabilities.IsAndroid());
  caps.Set("webauthn:extension:prf", !capabilities.IsAndroid());

  // See https://github.com/fedidcg/FedCM/pull/478
  caps.Set("fedcm:accounts", true);

  // Chrome-specific extensions.
  const std::string chrome_driver_version_key = base::StringPrintf(
      "%s.%sVersion", base::ToLowerASCII(kBrowserShortName).c_str(),
      base::ToLowerASCII(kChromeDriverProductShortName).c_str());
  caps.SetByDottedPath(chrome_driver_version_key, kChromeDriverVersion);
  if (session->chrome->GetBrowserInfo()->debugger_endpoint.IsValid()) {
    const std::string debugger_address_key = base::StringPrintf(
        "%s.debuggerAddress", kChromeDriverOptionsKeyPrefixed);
    caps.SetByDottedPath(debugger_address_key, session->chrome->GetBrowserInfo()
                                                   ->debugger_endpoint.Address()
                                                   .ToString());
  }
  ChromeDesktopImpl* desktop = nullptr;
  Status status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsOk()) {
    const std::string user_data_key = base::StringPrintf(
        "%s.userDataDir", base::ToLowerASCII(kBrowserShortName).c_str());
    caps.SetByDottedPath(
        user_data_key,
        desktop->command().GetSwitchValuePath("user-data-dir").AsUTF8Unsafe());
    caps.Set("networkConnectionEnabled", desktop->IsNetworkConnectionEnabled());
  }

  // Legacy capabilities.
  if (!session->w3c_compliant) {
    caps.Set("javascriptEnabled", true);
    caps.Set("takesScreenshot", true);
    caps.Set("takesHeapSnapshot", true);
    caps.Set("handlesAlerts", true);
    caps.Set("databaseEnabled", false);
    caps.Set("locationContextEnabled", true);
    caps.Set("mobileEmulationEnabled",
             session->chrome->IsMobileEmulationEnabled());
    caps.Set("browserConnectionEnabled", false);
    caps.Set("cssSelectorsEnabled", true);
    caps.Set("webStorageEnabled", true);
    caps.Set("rotatable", false);
    caps.Set("acceptSslCerts", capabilities.accept_insecure_certs);
    caps.Set("nativeEvents", true);
    caps.Set("hasTouchScreen", session->chrome->HasTouchScreen());
  }

  if (session->web_socket_url) {
    caps.Set("webSocketUrl",
             "ws://" + session->host + "/session/" + session->id);
  }

  return caps;
}

Status InitSessionHelper(const InitSessionParams& bound_params,
                         Session* session,
                         const base::Value::Dict& params,
                         std::unique_ptr<base::Value>* value) {
  if (!bound_params.device_manager) {
    return Status{kSessionNotCreated, "device manager cannot be null"};
  }

  const base::Value::Dict* desired_caps;
  base::Value::Dict merged_caps;

  Capabilities capabilities;
  Status status = internal::ConfigureSession(session, params, desired_caps,
                                             merged_caps, &capabilities);
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

  if (session->web_socket_url) {
    // Suffixes used with the client channels.
    std::string client_suffixes[] = {Session::kChannelSuffix,
                                     Session::kNoChannelSuffix,
                                     Session::kBlockingChannelSuffix};
    for (std::string suffix : client_suffixes) {
      BidiTracker* bidi_tracker = new BidiTracker();
      bidi_tracker->SetChannelSuffix(std::move(suffix));
      bidi_tracker->SetBidiCallback(base::BindRepeating(
          &Session::OnBidiResponse, base::Unretained(session)));
      devtools_event_listeners.emplace_back(bidi_tracker);
    }
  }

  status =
      LaunchChrome(bound_params.url_loader_factory, bound_params.socket_factory,
                   *bound_params.device_manager, capabilities,
                   std::move(devtools_event_listeners), session->w3c_compliant,
                   session->chrome);

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
  session->capabilities = std::make_unique<base::Value::Dict>(
      CreateCapabilities(session, capabilities, *desired_caps));

  status = internal::ConfigureHeadlessSession(session, capabilities);
  if (status.IsError())
    return status;

  if (session->w3c_compliant) {
    base::Value::Dict body;
    body.Set("capabilities", session->capabilities->Clone());
    body.Set("sessionId", session->id);
    *value = std::make_unique<base::Value>(body.Clone());
  } else {
    *value = std::make_unique<base::Value>(session->capabilities->Clone());
  }

  if (session->web_socket_url) {
    WebView* web_view = nullptr;
    status = session->GetTargetWindow(&web_view);
    if (status.IsError())
      return status;
    session->bidi_mapper_web_view_id = session->window;

    // Create a new tab because the default one will be occupied by the
    // mapper. The new tab is activated and focused.
    std::string web_view_id;
    status = session->chrome->NewWindow(
        session->window, Chrome::WindowType::kTab, false, &web_view_id);

    if (status.IsError()) {
      return status;
    }

    std::unique_ptr<base::Value> result;
    base::Value::Dict body;
    body.Set("handle", web_view_id);

    // Even though the new tab is already activated the explicit switch to the
    // new tab is needed to update the internal state of ChromeDriver properly.
    status = ExecuteSwitchToWindow(session, body, &result);
    if (status.IsError()) {
      return status;
    }

    // Wait until the default page navigation is over to prevent the mapper
    // from being evicted by the navigation.
    status = web_view->WaitForPendingNavigations(
        session->GetCurrentFrameId(), Timeout(session->page_load_timeout),
        true);
    if (status.IsError()) {
      return status;
    }

    // Start the mapper.
    base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    base::FilePath bidi_mapper_path =
        cmd_line->GetSwitchValuePath("bidi-mapper-path");

    std::string mapper_script = kMapperScript;

    if (!bidi_mapper_path.empty()) {
      VLOG(0) << "Custom BiDi mapper path specified: " << bidi_mapper_path;

      if (!base::ReadFileToString(bidi_mapper_path, &mapper_script)) {
        return Status(StatusCode::kUnknownError,
                      "Failed to read the specified BiDi mapper path: " +
                          bidi_mapper_path.AsUTF8Unsafe());
      }
    }

    status = web_view->StartBidiServer(mapper_script);
    if (status.IsError()) {
      return status;
    }

    // Execute session.new for the newly-created mapper instance.
    base::Value::Dict bidi_cmd;
    bidi_cmd.Set("channel", "/init-bidi-session");
    bidi_cmd.Set("id", 1);
    bidi_cmd.Set("params", params.Clone());
    bidi_cmd.Set("method", "session.new");
    base::Value::Dict bidi_response;
    status = web_view->SendBidiCommand(
        std::move(bidi_cmd), Timeout(base::Seconds(20)), bidi_response);
    if (status.IsError()) {
      return status;
    }
  }  // if (session->web_socket_url)

  return status;
}

}  // namespace

namespace internal {

Status ConfigureSession(Session* session,
                        const base::Value::Dict& params,
                        const base::Value::Dict*& desired_caps,
                        base::Value::Dict& merged_caps,
                        Capabilities* capabilities) {
  session->driver_log =
      std::make_unique<WebDriverLog>(WebDriverLog::kDriverType, Log::kAll);

  session->w3c_compliant = GetW3CSetting(params);
  if (session->w3c_compliant) {
    Status status = ProcessCapabilities(params, merged_caps);
    if (status.IsError())
      return status;
    desired_caps = &merged_caps;
  } else {
    const base::Value::Dict* caps = params.FindDict("desiredCapabilities");
    if (!caps)
      return Status(kSessionNotCreated, "Missing or invalid capabilities");
    desired_caps = caps;
  }

  Status status = capabilities->Parse(*desired_caps, session->w3c_compliant);
  if (status.IsError())
    return status;

  if (capabilities->unhandled_prompt_behavior) {
    session->unhandled_prompt_behavior =
        std::move(capabilities->unhandled_prompt_behavior).value();
  } else {
    session->unhandled_prompt_behavior = PromptBehavior(session->w3c_compliant);
  }

  session->implicit_wait = capabilities->implicit_wait_timeout;
  session->page_load_timeout = capabilities->page_load_timeout;
  session->script_timeout = capabilities->script_timeout;
  session->strict_file_interactability =
      capabilities->strict_file_interactability;
  session->web_socket_url = capabilities->web_socket_url;
  Log::Level driver_level = Log::kWarning;
  if (capabilities->logging_prefs.count(WebDriverLog::kDriverType))
    driver_level = capabilities->logging_prefs[WebDriverLog::kDriverType];
  session->driver_log->set_min_level(driver_level);

  return Status(kOk);
}

Status ConfigureHeadlessSession(Session* session,
                                const Capabilities& capabilities) {
  if (!session->chrome->GetBrowserInfo()->is_headless_shell) {
    return Status(kOk);
  }

  const std::string* download_directory = nullptr;
  if (capabilities.prefs) {
    download_directory = capabilities.prefs->FindStringByDottedPath(
        "download.default_directory");
    if (!download_directory) {
      download_directory =
          capabilities.prefs->FindString("download.default_directory");
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

bool MergeCapabilities(const base::Value::Dict& always_match,
                       const base::Value::Dict& first_match,
                       base::Value::Dict& merged) {
  merged.clear();

  for (auto kv : first_match) {
    if (always_match.Find(kv.first)) {
      // `first_match` cannot have the same `keys` as `always_match`.
      return false;
    }
  }

  // merge the capabilities together since guaranteed no key collisions
  merged = always_match.Clone();
  merged.Merge(first_match.Clone());
  return true;
}

// Implementation of "matching capabilities", as defined in W3C spec at
// https://www.w3.org/TR/webdriver/#dfn-matching-capabilities.
// It checks some requested capabilities and make sure they are supported.
// Currently, we only check "browserName", "platformName", "fedcm:accounts"
// and webauthn capabilities but more can be added as necessary.
bool MatchCapabilities(const base::Value::Dict& capabilities) {
  const base::Value* name = capabilities.Find("browserName");
  if (name && !name->is_none()) {
    if (!name->is_string()) {
      return false;
    }
    if (name->GetString() != kBrowserCapabilityName &&
        name->GetString() != kHeadlessShellCapabilityName) {
      return false;
    }
  }

  const base::Value::Dict* chrome_options;
  const bool has_chrome_options =
      GetChromeOptionsDictionary(capabilities, &chrome_options);

  bool is_android = has_chrome_options &&
                    chrome_options->FindString("androidPackage") != nullptr;

  const base::Value* platform_name_value = capabilities.Find("platformName");
  if (platform_name_value && !platform_name_value->is_none()) {
    if (!platform_name_value->is_string())
      return false;

    std::string requested_platform_name = platform_name_value->GetString();
    std::string requested_first_token =
        requested_platform_name.substr(0, requested_platform_name.find(' '));

    std::string actual_platform_name =
        base::ToLowerASCII(base::SysInfo::OperatingSystemName());
    std::string actual_first_token =
        actual_platform_name.substr(0, actual_platform_name.find(' '));

    bool is_remote = has_chrome_options &&
                     chrome_options->FindString("debuggerAddress") != nullptr;
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
  }

  const base::Value* virtual_authenticators_value =
      capabilities.Find("webauthn:virtualAuthenticators");
  if (virtual_authenticators_value) {
    if (!virtual_authenticators_value->is_bool() ||
        (virtual_authenticators_value->GetBool() && is_android)) {
      return false;
    }
  }

  const base::Value* large_blob_value =
      capabilities.Find("webauthn:extension:largeBlob");
  if (large_blob_value) {
    if (!large_blob_value->is_bool() ||
        (large_blob_value->GetBool() && is_android)) {
      return false;
    }
  }

  const base::Value* fedcm_accounts_value = capabilities.Find("fedcm:accounts");
  if (fedcm_accounts_value) {
    if (!fedcm_accounts_value->is_bool() || !fedcm_accounts_value->GetBool()) {
      return false;
    }
  }

  return true;
}

// Implementation of "process capabilities", as defined in W3C spec at
// https://www.w3.org/TR/webdriver/#processing-capabilities. Step numbers in
// the comments correspond to the step numbers in the spec.
Status ProcessCapabilities(const base::Value::Dict& params,
                           base::Value::Dict& result_capabilities) {
  // 1. Get the property "capabilities" from parameters.
  const base::Value::Dict* capabilities_request =
      params.FindDict("capabilities");
  if (!capabilities_request)
    return Status(kInvalidArgument, "'capabilities' must be a JSON object");

  // 2. Get the property "alwaysMatch" from capabilities request.
  const base::Value::Dict empty_object;
  const base::Value::Dict* required_capabilities;
  const base::Value* required_capabilities_value =
      capabilities_request->Find("alwaysMatch");
  if (required_capabilities_value == nullptr) {
    required_capabilities = &empty_object;
  } else if (required_capabilities_value->is_dict()) {
    required_capabilities = &required_capabilities_value->GetDict();
    Capabilities cap;
    Status status = cap.Parse(*required_capabilities);
    if (status.IsError())
      return status;
  } else {
    return Status(kInvalidArgument, "'alwaysMatch' must be a JSON object");
  }

  // 3. Get the property "firstMatch" from capabilities request.
  base::Value::List default_list;
  const base::Value::List* all_first_match_capabilities;
  const base::Value* all_first_match_capabilities_value =
      capabilities_request->Find("firstMatch");
  if (all_first_match_capabilities_value == nullptr) {
    default_list.Append(base::Value::Dict());
    all_first_match_capabilities = &default_list;
  } else if (all_first_match_capabilities_value->is_list()) {
    all_first_match_capabilities =
        &all_first_match_capabilities_value->GetList();
    if (all_first_match_capabilities->size() < 1) {
      return Status(kInvalidArgument,
                    "'firstMatch' must contain at least one entry");
    }
  } else {
    return Status(kInvalidArgument, "'firstMatch' must be a JSON list");
  }

  // 4. Let validated first match capabilities be an empty JSON List.
  std::vector<const base::Value::Dict*> validated_first_match_capabilities;

  // 5. Validate all first match capabilities.
  for (size_t i = 0; i < all_first_match_capabilities->size(); ++i) {
    const base::Value& first_match = (*all_first_match_capabilities)[i];
    if (!first_match.is_dict()) {
      return Status(kInvalidArgument,
                    base::StringPrintf(
                        "entry %zu of 'firstMatch' must be a JSON object", i));
    }
    Capabilities cap;
    Status status = cap.Parse(first_match.GetDict());
    if (status.IsError())
      return Status(
          kInvalidArgument,
          base::StringPrintf("entry %zu of 'firstMatch' is invalid", i),
          status);
    validated_first_match_capabilities.push_back(&first_match.GetDict());
  }

  // 6. Let merged capabilities be an empty List.
  std::vector<base::Value::Dict> merged_capabilities;

  // 7. Merge capabilities.
  for (size_t i = 0; i < validated_first_match_capabilities.size(); ++i) {
    const base::Value::Dict* first_match_capabilities =
        validated_first_match_capabilities[i];
    base::Value::Dict merged;
    if (!MergeCapabilities(*required_capabilities, *first_match_capabilities,
                           merged)) {
      return Status(
          kInvalidArgument,
          base::StringPrintf(
              "unable to merge 'alwaysMatch' with entry %zu of 'firstMatch'",
              i));
    }
    merged_capabilities.emplace_back(std::move(merged));
  }

  // 8. Match capabilities.
  for (auto& capabilities : merged_capabilities) {
    if (MatchCapabilities(capabilities)) {
      result_capabilities = std::move(capabilities);
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
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value) {
  Status status = InitSessionHelper(bound_params, session, params, value);
  if (status.IsError()) {
    session->quit = true;
    if (session->chrome != nullptr)
      session->chrome->Quit();
    return status;
  }

  return status;
}

Status ExecuteQuit(bool allow_detach,
                   Session* session,
                   const base::Value::Dict& params,
                   std::unique_ptr<base::Value>* value) {
  session->quit = true;
  if (allow_detach && session->detach)
    return Status(kOk);
  return session->chrome->Quit();
}

// Quits a session.
Status ExecuteBidiSessionEnd(Session* session,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value) {
  Status status{kOk};
  WebView* web_view = nullptr;
  status = session->chrome->GetWebViewById(session->bidi_mapper_web_view_id,
                                           &web_view);
  if (status.IsOk()) {
    // Attempting to forward any pending BiDi responses / events.
    status = web_view->HandleReceivedEvents();
  }

  if (status.IsError()) {
    VLOG(0) << "Ignoring the error while shutting down a BiDi session: "
            << status.message();
  }

  session->quit = true;
  status = session->chrome->Quit();
  if (status.IsOk()) {
    *value = std::make_unique<base::Value>(base::Value::Type::DICT);
  }
  return status;
}

Status ExecuteGetSessionCapabilities(Session* session,
                                     const base::Value::Dict& params,
                                     std::unique_ptr<base::Value>* value) {
  *value = std::make_unique<base::Value>(session->capabilities->Clone());
  return Status(kOk);
}

Status ExecuteGetCurrentWindowHandle(Session* session,
                                     const base::Value::Dict& params,
                                     std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;
  *value = std::make_unique<base::Value>(web_view->GetId());
  return Status(kOk);
}

Status ExecuteClose(Session* session,
                    const base::Value::Dict& params,
                    std::unique_ptr<base::Value>* value) {
  std::list<std::string> web_view_ids;
  Status status = session->chrome->GetWebViewIds(&web_view_ids,
                                                 session->w3c_compliant);
  if (status.IsError())
    return status;
  bool is_last_web_view = web_view_ids.size() == 1u;
  if (session->web_socket_url) {
    is_last_web_view = web_view_ids.size() <= 2u;
  }
  web_view_ids.clear();

  WebView* web_view = nullptr;
  status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  status = web_view->HandleReceivedEvents();
  if (status.IsError())
    return status;

  if (web_view->IsDialogOpen()) {
    std::string alert_text;
    status = web_view->GetDialogMessage(alert_text);
    if (status.IsError())
      return status;

    std::string dialog_type;
    status = web_view->GetTypeOfDialog(dialog_type);
    if (status.IsError()) {
      return status;
    }

    PromptHandlerConfiguration prompt_handler_configuration;
    status = session->unhandled_prompt_behavior.GetConfiguration(
        dialog_type, prompt_handler_configuration);
    if (status.IsError()) {
      return status;
    }

    if (prompt_handler_configuration.type == PromptHandlerType::kAccept ||
        prompt_handler_configuration.type == PromptHandlerType::kDismiss) {
      status = web_view->HandleDialog(
          prompt_handler_configuration.type == PromptHandlerType::kAccept,
          session->prompt_text);
      if (status.IsError()) {
        return status;
      }
    }

    if (prompt_handler_configuration.notify) {
      return Status(kUnexpectedAlertOpen, "{Alert text : " + alert_text + "}");
    }
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
    status = ExecuteGetWindowHandles(session, base::Value::Dict(), value);
    if (status.IsError())
      return status;
  }

  return status;
}

Status ExecuteGetWindowHandles(Session* session,
                               const base::Value::Dict& params,
                               std::unique_ptr<base::Value>* value) {
  std::list<std::string> web_view_ids;
  Status status = session->chrome->GetWebViewIds(&web_view_ids,
                                                 session->w3c_compliant);
  if (status.IsError()) {
    return status;
  }

  if (session->web_socket_url) {
    auto it =
        base::ranges::find(web_view_ids, session->bidi_mapper_web_view_id);
    if (it != web_view_ids.end()) {
      web_view_ids.erase(it);
    }
  }

  base::Value::List window_ids;
  for (std::list<std::string>::const_iterator it = web_view_ids.begin();
       it != web_view_ids.end(); ++it) {
    window_ids.Append(*it);
  }
  *value = std::make_unique<base::Value>(std::move(window_ids));
  return Status(kOk);
}

Status ExecuteSwitchToWindow(Session* session,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value) {
  const std::string* name;
  if (session->w3c_compliant) {
    name = params.FindString("handle");
    if (!name)
      return Status(kInvalidArgument, "'handle' must be a string");
  } else {
    name = params.FindString("name");
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
  // Check if any web_view matches |name|.
  for (std::list<std::string>::const_iterator it = web_view_ids.begin();
       it != web_view_ids.end(); ++it) {
    if (*it == *name) {
      web_view_id = *name;
      found = true;
      break;
    }
  }
  if (!found) {
    // Check if any of the tab window names match |name|.
    const char* kGetWindowNameScript = "function() { return window.name; }";
    base::Value::List args;
    for (std::list<std::string>::const_iterator it = web_view_ids.begin();
         it != web_view_ids.end(); ++it) {
      std::unique_ptr<base::Value> result;
      WebView* web_view;
      status = session->chrome->GetWebViewById(*it, &web_view);
      // `CallFunction(...)` below may remove web views that detach during this
      // loop. In that case, continue searching.
      if (status.IsError())
        continue;
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
                               const base::Value::Dict& params,
                               std::unique_ptr<base::Value>* value) {
  std::optional<double> maybe_ms = params.FindDouble("ms");
  if (!maybe_ms.has_value())
    return Status(kInvalidArgument, "'ms' must be a double");

  const std::string* type = params.FindString("type");
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
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value) {
  for (auto setting : params) {
    int64_t timeout_ms_int64 = -1;
    base::TimeDelta timeout;
    const std::string& type = setting.first;
    if (setting.second.is_none()) {
      if (type != "script")
        return Status(kInvalidArgument, "timeout can not be null");
      timeout = base::TimeDelta::Max();
    } else {
      if (!GetOptionalSafeInt(params, setting.first, &timeout_ms_int64) ||
          timeout_ms_int64 < 0) {
        return Status(kInvalidArgument, "value must be a non-negative integer");
      }
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
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value) {
  // TODO(crbug.com/chromedriver/2596): Remove legacy version support when we
  // stop supporting non-W3C protocol. At that time, we can delete the legacy
  // function and merge the W3C function into this function.
  if (params.contains("ms"))
    return ExecuteSetTimeoutLegacy(session, params, value);
  return ExecuteSetTimeoutsW3C(session, params, value);
}

Status ExecuteGetTimeouts(Session* session,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value) {
  base::Value::Dict timeouts;
  if (session->script_timeout == base::TimeDelta::Max())
    timeouts.Set("script", base::Value());
  else
    SetSafeInt(timeouts, "script", session->script_timeout.InMilliseconds());

  SetSafeInt(timeouts, "pageLoad", session->page_load_timeout.InMilliseconds());
  SetSafeInt(timeouts, "implicit", session->implicit_wait.InMilliseconds());

  *value = base::Value::ToUniquePtrValue(base::Value(std::move(timeouts)));
  return Status(kOk);
}

Status ExecuteSetScriptTimeout(Session* session,
                               const base::Value::Dict& params,
                               std::unique_ptr<base::Value>* value) {
  std::optional<double> maybe_ms = params.FindDouble("ms");
  if (!maybe_ms.has_value() || maybe_ms.value() < 0)
    return Status(kInvalidArgument, "'ms' must be a non-negative number");
  session->script_timeout =
      base::Milliseconds(static_cast<int>(maybe_ms.value()));
  return Status(kOk);
}

Status ExecuteImplicitlyWait(Session* session,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value) {
  std::optional<double> maybe_ms = params.FindDouble("ms");
  if (!maybe_ms.has_value() || maybe_ms.value() < 0)
    return Status(kInvalidArgument, "'ms' must be a non-negative number");
  session->implicit_wait =
      base::Milliseconds(static_cast<int>(maybe_ms.value()));
  return Status(kOk);
}

Status ExecuteIsLoading(Session* session,
                        const base::Value::Dict& params,
                        std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  bool is_pending;
  status = web_view->IsPendingNavigation(nullptr, &is_pending);
  if (status.IsError())
    return status;
  *value = std::make_unique<base::Value>(is_pending);
  return Status(kOk);
}

Status ExecuteCreateVirtualSensor(Session* session,
                                  const base::Value::Dict& params,
                                  std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError()) {
    return status;
  }

  const std::string* type = params.FindString("type");
  if (!type) {
    return Status(kInvalidArgument, "'type' must be a string");
  }

  base::Value::Dict args;
  args.Set("enabled", true);
  args.Set("type", *type);

  base::Value::Dict metadata;
  metadata.Set("available", params.FindBool("connected").value_or(true));
  if (auto minimum_sampling_frequency =
          params.FindDouble("minSamplingFrequency");
      minimum_sampling_frequency) {
    metadata.Set("minimumFrequency", minimum_sampling_frequency.value());
  }
  if (auto maximum_sampling_frequency =
          params.FindDouble("maxSamplingFrequency");
      maximum_sampling_frequency) {
    metadata.Set("maximumFrequency", maximum_sampling_frequency.value());
  }
  args.Set("metadata", std::move(metadata));

  return web_view->SendCommand("Emulation.setSensorOverrideEnabled", args);
}

namespace {

bool ParseSingleValue(const std::string& key_name,
                      const base::Value::Dict& params,
                      base::Value::Dict* out_params) {
  std::optional<double> value = params.FindDouble(key_name);
  if (!value.has_value()) {
    return false;
  }
  // Construct a dict that looks like this:
  // {
  //   single: {
  //     value: VAL
  //   }
  // }
  out_params->Set("single", base::Value::Dict().Set("value", *value));
  return true;
}

bool ParseXYZValue(const base::Value::Dict& params,
                   base::Value::Dict* out_params) {
  std::optional<double> x = params.FindDouble("x");
  if (!x.has_value()) {
    return false;
  }
  std::optional<double> y = params.FindDouble("y");
  if (!y.has_value()) {
    return false;
  }
  std::optional<double> z = params.FindDouble("z");
  if (!z.has_value()) {
    return false;
  }
  // Construct a dict that looks like this:
  // {
  //   xyz: {
  //     x: VAL1,
  //     y: VAL2,
  //     z: VAL3
  //   }
  // }
  out_params->Set("xyz",
                  base::Value::Dict().Set("x", *x).Set("y", *y).Set("z", *z));
  return true;
}

bool ParseOrientationEuler(const base::Value::Dict& params,
                           base::Value::Dict* out_params) {
  if (!params.contains("alpha") || !params.contains("beta") ||
      !params.contains("gamma")) {
    return false;
  }

  std::optional<double> alpha = params.FindDouble("alpha");
  if (!alpha.has_value()) {
    return false;
  }
  std::optional<double> beta = params.FindDouble("beta");
  if (!beta.has_value()) {
    return false;
  }
  std::optional<double> gamma = params.FindDouble("gamma");
  if (!gamma.has_value()) {
    return false;
  }
  device::SensorReading quaternion_readings;
  if (!device::ComputeQuaternionFromEulerAngles(*alpha, *beta, *gamma,
                                                &quaternion_readings)) {
    return false;
  }

  // Construct a dict that looks like this:
  // {
  //   quaternion: {
  //     x: VAL1,
  //     y: VAL2,
  //     z: VAL3,
  //     w: VAL4
  //   }
  // }
  const double x = quaternion_readings.orientation_quat.x;
  const double y = quaternion_readings.orientation_quat.y;
  const double z = quaternion_readings.orientation_quat.z;
  const double w = quaternion_readings.orientation_quat.w;
  out_params->Set(
      "quaternion",
      base::Value::Dict().Set("x", x).Set("y", y).Set("z", z).Set("w", w));
  return true;
}

base::expected<base::Value::Dict, Status> ParseSensorUpdateParams(
    const base::Value::Dict& params) {
  base::Value::Dict cdp_params;

  const std::string* type = params.FindString("type");
  if (!type) {
    return base::unexpected(
        Status(kInvalidArgument, "'type' must be a string"));
  }
  cdp_params.Set("type", *type);

  const base::Value::Dict* reading_dict = params.FindDict("reading");
  if (!reading_dict) {
    return base::unexpected(
        Status(kInvalidArgument, "Missing 'reading' field"));
  }

  base::Value::Dict reading;
  if (*type == "ambient-light") {
    if (!ParseSingleValue("illuminance", *reading_dict, &reading)) {
      return base::unexpected(
          Status(kInvalidArgument, "Could not parse illuminance"));
    }
  } else if (*type == "accelerometer" || *type == "gravity" ||
             *type == "gyroscope" || *type == "linear-acceleration" ||
             *type == "magnetometer") {
    if (!ParseXYZValue(*reading_dict, &reading)) {
      return base::unexpected(
          Status(kInvalidArgument, "Could not parse XYZ fields"));
    }
  } else if (*type == "absolute-orientation" ||
             *type == "relative-orientation") {
    if (!ParseOrientationEuler(*reading_dict, &reading)) {
      return base::unexpected(Status(
          kInvalidArgument, "Could not parse " + *type +
                                " readings. Invalid alpha/beta/gamma values"));
    }
  } else {
    return base::unexpected(Status(
        kInvalidArgument, "Unexpected type " + *type + " in 'type' field"));
  }
  cdp_params.Set("reading", std::move(reading));

  return cdp_params;
}

}  // namespace

Status ExecuteUpdateVirtualSensor(Session* session,
                                  const base::Value::Dict& params,
                                  std::unique_ptr<base::Value>*) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError()) {
    return status;
  }

  auto cdp_params = ParseSensorUpdateParams(params);
  if (!cdp_params.has_value()) {
    return cdp_params.error();
  }

  return web_view->SendCommand("Emulation.setSensorOverrideReadings",
                               cdp_params.value());
}

Status ExecuteRemoveVirtualSensor(Session* session,
                                  const base::Value::Dict& params,
                                  std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError()) {
    return status;
  }

  const std::string* type = params.FindString("type");

  if (!type) {
    return Status(kInvalidArgument, "'type' must be a string");
  }

  base::Value::Dict args;
  args.Set("enabled", false);
  args.Set("type", *type);

  return web_view->SendCommand("Emulation.setSensorOverrideEnabled", args);
}

Status ExecuteGetVirtualSensorInformation(Session* session,
                                          const base::Value::Dict& params,
                                          std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError()) {
    return status;
  }

  const std::string* type = params.FindString("type");
  if (!type) {
    return Status(kInvalidArgument, "'type' must be a string");
  }

  base::Value::Dict args;
  args.Set("type", *type);

  return web_view->SendCommandAndGetResult(
      "Emulation.getOverriddenSensorInformation", args, value);
}

Status ExecuteGetLocation(Session* session,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value) {
  if (!session->overridden_geoposition) {
    return Status(kUnknownError,
                  "Location must be set before it can be retrieved");
  }
  base::Value::Dict location;
  location.Set("latitude", session->overridden_geoposition->latitude);
  location.Set("longitude", session->overridden_geoposition->longitude);
  location.Set("accuracy", session->overridden_geoposition->accuracy);
  // Set a dummy altitude to make WebDriver clients happy.
  // https://code.google.com/p/chromedriver/issues/detail?id=281
  location.Set("altitude", 0);
  *value = base::Value::ToUniquePtrValue(base::Value(std::move(location)));
  return Status(kOk);
}

Status ExecuteGetNetworkConnection(Session* session,
                                   const base::Value::Dict& params,
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
                                   const base::Value::Dict& params,
                                   std::unique_ptr<base::Value>* value) {
  if (!session->overridden_network_conditions) {
    return Status(kUnknownError,
                  "network conditions must be set before it can be retrieved");
  }
  base::Value::Dict conditions =
      base::Value::Dict()
          .Set("offline", session->overridden_network_conditions->offline)
          .Set("latency", session->overridden_network_conditions->latency)
          .Set("download_throughput",
               session->overridden_network_conditions->download_throughput)
          .Set("upload_throughput",
               session->overridden_network_conditions->upload_throughput);
  *value = base::Value::ToUniquePtrValue(base::Value(std::move(conditions)));
  return Status(kOk);
}

Status ExecuteSetNetworkConnection(Session* session,
                                   const base::Value::Dict& params,
                                   std::unique_ptr<base::Value>* value) {
  ChromeDesktopImpl* desktop = nullptr;
  Status status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsError())
    return status;
  if (!desktop->IsNetworkConnectionEnabled())
    return Status(kUnknownError, "network connection must be enabled");

  std::optional<int> connection_type =
      params.FindIntByDottedPath("parameters.type");
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
                                const base::Value::Dict& params,
                                std::unique_ptr<base::Value>* value) {
  Chrome::WindowRect window_rect;
  Status status = session->chrome->GetWindowRect(session->window, &window_rect);

  if (status.IsError())
    return status;

  base::Value::Dict position =
      base::Value::Dict().Set("x", window_rect.x).Set("y", window_rect.y);
  *value = base::Value::ToUniquePtrValue(base::Value(std::move(position)));
  return Status(kOk);
}

Status ExecuteSetWindowPosition(Session* session,
                                const base::Value::Dict& params,
                                std::unique_ptr<base::Value>* value) {
  std::optional<double> maybe_x = params.FindDouble("x");
  std::optional<double> maybe_y = params.FindDouble("y");

  if (!maybe_x.has_value() || !maybe_y.has_value())
    return Status(kInvalidArgument, "missing or invalid 'x' or 'y'");

  base::Value::Dict rect_params;
  rect_params.Set("x", static_cast<int>(maybe_x.value()));
  rect_params.Set("y", static_cast<int>(maybe_y.value()));
  return session->chrome->SetWindowRect(session->window,
                                        std::move(rect_params));
}

Status ExecuteGetWindowSize(Session* session,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value) {
  Chrome::WindowRect window_rect;
  Status status = session->chrome->GetWindowRect(session->window, &window_rect);

  if (status.IsError())
    return status;

  base::Value::Dict size = base::Value::Dict()
                               .Set("width", window_rect.width)
                               .Set("height", window_rect.height);
  *value = base::Value::ToUniquePtrValue(base::Value(std::move(size)));
  return Status(kOk);
}

Status ExecuteSetWindowSize(Session* session,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value) {
  std::optional<double> maybe_width = params.FindDouble("width");
  std::optional<double> maybe_height = params.FindDouble("height");

  if (!maybe_width.has_value() || !maybe_height.has_value())
    return Status(kInvalidArgument, "missing or invalid 'width' or 'height'");

  base::Value::Dict rect_params;
  rect_params.Set("width", static_cast<int>(maybe_width.value()));
  rect_params.Set("height", static_cast<int>(maybe_height.value()));
  return session->chrome->SetWindowRect(session->window,
                                        std::move(rect_params));
}

Status ExecuteGetAvailableLogTypes(Session* session,
                                   const base::Value::Dict& params,
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
                     const base::Value::Dict& params,
                     std::unique_ptr<base::Value>* value) {
  const std::string* log_type = params.FindString("type");
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
      *value = base::Value::ToUniquePtrValue(
          base::Value((*log)->GetAndClearEntries()));
      return Status(kOk);
    }
  }
  return Status(kInvalidArgument, "log type '" + *log_type + "' not found");
}

Status ExecuteUploadFile(Session* session,
                         const base::Value::Dict& params,
                         std::unique_ptr<base::Value>* value) {
  const std::string* base64_zip_data = params.FindString("file");
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

Status ExecuteSetSPCTransactionMode(Session* session,
                                    const base::Value::Dict& params,
                                    std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  const std::string* mode = params.FindString("mode");
  if (!mode)
    return Status(kInvalidArgument, "missing parameter 'mode'");

  base::Value::Dict body;
  body.Set("mode", *mode);

  return web_view->SendCommandAndGetResult("Page.setSPCTransactionMode", body,
                                           value);
}

Status ExecuteGenerateTestReport(Session* session,
                                 const base::Value::Dict& params,
                                 std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  const std::string* message = params.FindString("message");
  if (!message)
    return Status(kInvalidArgument, "missing parameter 'message'");
  const std::string* group = params.FindString("group");

  base::Value::Dict body;
  body.Set("message", *message);
  body.Set("group", group ? *group : "default");

  web_view->SendCommandAndGetResult("Page.generateTestReport", body, value);
  return Status(kOk);
}

Status ExecuteSetTimeZone(Session* session,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  const std::string* time_zone = params.FindString("time_zone");
  if (!time_zone)
    return Status(kInvalidArgument, "missing parameter 'time_zone'");

  base::Value::Dict body;
  body.Set("timezoneId", *time_zone);

  web_view->SendCommandAndGetResult("Emulation.setTimezoneOverride", body,
                                    value);
  return Status(kOk);
}

Status ExecuteCreateVirtualPressureSource(Session* session,
                                          const base::Value::Dict& params,
                                          std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError()) {
    return status;
  }

  const std::string* type = params.FindString("type");
  if (!type) {
    return Status(kInvalidArgument, "'type' must be a string");
  }

  base::Value::Dict body;
  body.Set("enabled", true);
  body.Set("source", *type);

  base::Value::Dict metadata;
  metadata.Set("available", true);
  if (params.contains("supported")) {
    auto supported = params.FindBool("supported");
    if (!supported.has_value()) {
      return Status(kInvalidArgument, "'supported' must be a boolean");
    }
    metadata.Set("available", *supported);
  }
  body.Set("metadata", std::move(metadata));

  return web_view->SendCommand("Emulation.setPressureSourceOverrideEnabled",
                               body);
}

Status ExecuteUpdateVirtualPressureSource(Session* session,
                                          const base::Value::Dict& params,
                                          std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError()) {
    return status;
  }

  const std::string* type = params.FindString("type");
  if (!type) {
    return Status(kInvalidArgument, "'type' must be a string");
  }

  const std::string* sample = params.FindString("sample");
  if (!sample) {
    return Status(kInvalidArgument, "'sample' must be a string");
  }

  base::Value::Dict body;
  body.Set("source", *type);
  body.Set("state", *sample);
  return web_view->SendCommand("Emulation.setPressureStateOverride", body);
}

Status ExecuteRemoveVirtualPressureSource(Session* session,
                                          const base::Value::Dict& params,
                                          std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError()) {
    return status;
  }

  const std::string* type = params.FindString("type");
  if (!type) {
    return Status(kInvalidArgument, "'type' must be a string");
  }

  base::Value::Dict body;
  body.Set("enabled", false);
  body.Set("source", *type);
  return web_view->SendCommand("Emulation.setPressureSourceOverrideEnabled",
                               body);
}

// Run a BiDi command
Status ForwardBidiCommand(Session* session,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value) {
  // session == nullptr is a valid case: ExecuteQuit has already been handled
  // in the session thread but the following
  // TerminateSessionThreadOnCommandThread has not yet been executed (the later
  // destroys the session thread) The connection has already been accepted by
  // the CMD thread but soon it will be closed. We don't need to do anything.
  if (session == nullptr) {
    return Status{kInvalidArgument, "session not found"};
  }
  const base::Value::Dict* data = params.FindDict("bidiCommand");
  if (!data) {
    return Status{kUnknownError, "bidiCommand is missing in params"};
  }

  std::optional<int> connection_id = params.FindInt("connectionId");
  if (!connection_id) {
    return Status{kUnknownCommand, "connectionId is missing in params"};
  }

  WebView* web_view = nullptr;
  Status status = session->chrome->GetWebViewById(
      session->bidi_mapper_web_view_id, &web_view);
  if (status.IsError()) {
    return status;
  }

  base::Value::Dict bidi_cmd = data->Clone();
  std::string* method = bidi_cmd.FindString("method");

  std::string* user_channel = bidi_cmd.FindString("channel");
  std::string channel;
  if (user_channel) {
    channel = *user_channel + "/" + base::NumberToString(*connection_id) +
              Session::kChannelSuffix;
  } else {
    channel =
        "/" + base::NumberToString(*connection_id) + Session::kNoChannelSuffix;
  }

  if (*method == "browsingContext.close") {
    bidi_cmd.Set("channel", channel + Session::kBlockingChannelSuffix);
    // Closing of the context is handled in a blocking way.
    // This simplifies us closing the browser if the last tab was closed.
    session->awaiting_bidi_response = true;
    status = web_view->PostBidiCommand(std::move(bidi_cmd));
    if (status.IsError()) {
      return status;
    }

    // The timeout is the same as in ChromeImpl::CloseTarget
    status = web_view->HandleEventsUntil(
        std::move(BidiResponseIsReceivedCallback(session)),
        Timeout(base::Seconds(20)));
    if (status.code() == kTimeout) {
      // It looks like something is going wrong with the BiDiMapper.
      // Terminating the session...
      session->quit = true;
      status = session->chrome->Quit();
      return Status(kUnknownError, "failed to close window in 20 seconds");
    }
    if (status.IsError()) {
      return status;
    }

    size_t web_view_count;
    status = session->chrome->GetWebViewCount(&web_view_count,
                                              session->w3c_compliant);
    if (status.IsError()) {
      return status;
    }

    bool is_last_web_view = web_view_count <= 1u;
    if (is_last_web_view) {
      session->quit = true;
      status = session->chrome->Quit();
    }
  } else {
    bidi_cmd.Set("channel", std::move(channel));
    status = web_view->PostBidiCommand(std::move(bidi_cmd));
  }

  return status;
}
