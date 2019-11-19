// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/server/http_handler.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"  // For CHECK macros.
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/alert_commands.h"
#include "chrome/test/chromedriver/chrome/adb_impl.h"
#include "chrome/test/chromedriver/chrome/device_manager.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/constants/version.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/session_thread_map.h"
#include "chrome/test/chromedriver/util.h"
#include "chrome/test/chromedriver/webauthn_commands.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/transitional_url_loader_factory_owner.h"
#include "url/url_util.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

const char kCreateWebSocketPath[] =
    "session/:sessionId/chromium/create_websocket";
const char kSendCommandFromWebSocket[] =
    "session/:sessionId/chromium/send_command_from_websocket";

namespace {

const char kLocalStorage[] = "localStorage";
const char kSessionStorage[] = "sessionStorage";
const char kShutdownPath[] = "shutdown";

bool w3cMode(const std::string& session_id,
             const SessionThreadMap& session_thread_map) {
  if (session_id.length() > 0 && session_thread_map.count(session_id) > 0)
    return session_thread_map.at(session_id)->w3cMode();
  return kW3CDefault;
}

}  // namespace

// WrapperURLLoaderFactory subclasses mojom::URLLoaderFactory as non-mojo, cross
// thread class. It basically posts ::CreateLoaderAndStart calls over to the UI
// thread, to call them on the real mojo object.
class WrapperURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  WrapperURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : url_loader_factory_(std::move(url_loader_factory)),
        network_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    if (network_task_runner_->RunsTasksInCurrentSequence()) {
      url_loader_factory_->CreateLoaderAndStart(
          std::move(loader), routing_id, request_id, options, request,
          std::move(client), traffic_annotation);
    } else {
      network_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&WrapperURLLoaderFactory::CreateLoaderAndStart,
                         base::Unretained(this), std::move(loader), routing_id,
                         request_id, options, request, std::move(client),
                         traffic_annotation));
    }
  }
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory)
      override {
    NOTIMPLEMENTED();
  }

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Runner for URLRequestContextGetter network thread.
  scoped_refptr<base::SequencedTaskRunner> network_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WrapperURLLoaderFactory);
};

CommandMapping::CommandMapping(HttpMethod method,
                               const std::string& path_pattern,
                               const Command& command)
    : method(method), path_pattern(path_pattern), command(command) {}

CommandMapping::CommandMapping(const CommandMapping& other) = default;

CommandMapping::~CommandMapping() {}

// Create a command mapping with a prefixed HTTP path (.e.g goog/).
CommandMapping VendorPrefixedCommandMapping(HttpMethod method,
                                            const char* path_pattern,
                                            const Command& command) {
  return CommandMapping(
      method, base::StringPrintf(path_pattern, kChromeDriverCompanyPrefix),
      command);
}

HttpHandler::HttpHandler(const std::string& url_base)
    : url_base_(url_base),
      received_shutdown_(false),
      command_map_(new CommandMap()) {}

HttpHandler::HttpHandler(
    const base::Closure& quit_func,
    const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const std::string& url_base,
    int adb_port)
    : quit_func_(quit_func), url_base_(url_base), received_shutdown_(false) {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif
  context_getter_ = new URLRequestContextGetter(io_task_runner);
  socket_factory_ = CreateSyncWebSocketFactory(context_getter_.get());
  adb_.reset(new AdbImpl(io_task_runner, adb_port));
  device_manager_.reset(new DeviceManager(adb_.get()));
  url_loader_factory_owner_ =
      std::make_unique<network::TransitionalURLLoaderFactoryOwner>(
          context_getter_.get());

  wrapper_url_loader_factory_ = std::make_unique<WrapperURLLoaderFactory>(
      url_loader_factory_owner_->GetURLLoaderFactory());
  CommandMapping commands[] = {
      //
      // W3C standard endpoints
      //
      CommandMapping(
          kPost, internal::kNewSessionPathPattern,
          base::BindRepeating(
              &ExecuteCreateSession, &session_thread_map_,
              WrapToCommand("InitSession",
                            base::BindRepeating(
                                &ExecuteInitSession,
                                InitSessionParams(
                                    wrapper_url_loader_factory_.get(),
                                    socket_factory_, device_manager_.get()))))),
      CommandMapping(kDelete, "session/:sessionId",
                     base::BindRepeating(
                         &ExecuteSessionCommand, &session_thread_map_, "Quit",
                         base::BindRepeating(&ExecuteQuit, false), true, true)),
      CommandMapping(kGet, "status", base::BindRepeating(&ExecuteGetStatus)),
      CommandMapping(kGet, "session/:sessionId/timeouts",
                     WrapToCommand("GetTimeouts",
                                   base::BindRepeating(&ExecuteGetTimeouts))),
      CommandMapping(kPost, "session/:sessionId/timeouts",
                     WrapToCommand("SetTimeouts",
                                   base::BindRepeating(&ExecuteSetTimeouts))),
      CommandMapping(
          kPost, "session/:sessionId/url",
          WrapToCommand("Navigate", base::BindRepeating(&ExecuteGet))),
      CommandMapping(
          kGet, "session/:sessionId/url",
          WrapToCommand("GetUrl", base::BindRepeating(&ExecuteGetCurrentUrl))),
      CommandMapping(
          kPost, "session/:sessionId/back",
          WrapToCommand("GoBack", base::BindRepeating(&ExecuteGoBack))),
      CommandMapping(
          kPost, "session/:sessionId/forward",
          WrapToCommand("GoForward", base::BindRepeating(&ExecuteGoForward))),
      CommandMapping(
          kPost, "session/:sessionId/refresh",
          WrapToCommand("Refresh", base::BindRepeating(&ExecuteRefresh))),

      CommandMapping(
          kGet, "session/:sessionId/title",
          WrapToCommand("GetTitle", base::BindRepeating(&ExecuteGetTitle))),
      CommandMapping(
          kGet, "session/:sessionId/window",
          WrapToCommand("GetWindow",
                        base::BindRepeating(&ExecuteGetCurrentWindowHandle))),
      CommandMapping(
          kDelete, "session/:sessionId/window",
          WrapToCommand("CloseWindow", base::BindRepeating(&ExecuteClose))),
      CommandMapping(
          kPost, "session/:sessionId/window/new",
          WrapToCommand("NewWindow", base::BindRepeating(&ExecuteNewWindow))),
      CommandMapping(
          kPost, "session/:sessionId/window",
          WrapToCommand("SwitchToWindow",
                        base::BindRepeating(&ExecuteSwitchToWindow))),
      CommandMapping(
          kGet, "session/:sessionId/window/handles",
          WrapToCommand("GetWindows",
                        base::BindRepeating(&ExecuteGetWindowHandles))),
      CommandMapping(kPost, "session/:sessionId/frame",
                     WrapToCommand("SwitchToFrame",
                                   base::BindRepeating(&ExecuteSwitchToFrame))),
      CommandMapping(
          kPost, "session/:sessionId/frame/parent",
          WrapToCommand("SwitchToParentFrame",
                        base::BindRepeating(&ExecuteSwitchToParentFrame))),
      CommandMapping(kGet, "session/:sessionId/window/rect",
                     WrapToCommand("GetWindowRect",
                                   base::BindRepeating(&ExecuteGetWindowRect))),
      CommandMapping(kPost, "session/:sessionId/window/rect",
                     WrapToCommand("SetWindowRect",
                                   base::BindRepeating(&ExecuteSetWindowRect))),

      CommandMapping(
          kPost, "session/:sessionId/window/maximize",
          WrapToCommand("MaximizeWindow",
                        base::BindRepeating(&ExecuteMaximizeWindow))),
      CommandMapping(
          kPost, "session/:sessionId/window/minimize",
          WrapToCommand("MinimizeWindow",
                        base::BindRepeating(&ExecuteMinimizeWindow))),

      CommandMapping(
          kPost, "session/:sessionId/window/fullscreen",
          WrapToCommand("FullscreenWindow",
                        base::BindRepeating(&ExecuteFullScreenWindow))),

      CommandMapping(
          kGet, "session/:sessionId/element/active",
          WrapToCommand("GetActiveElement",
                        base::BindRepeating(&ExecuteGetActiveElement))),
      CommandMapping(
          kPost, "session/:sessionId/element",
          WrapToCommand("FindElement",
                        base::BindRepeating(&ExecuteFindElement, 50))),
      CommandMapping(
          kPost, "session/:sessionId/elements",
          WrapToCommand("FindElements",
                        base::BindRepeating(&ExecuteFindElements, 50))),
      CommandMapping(
          kPost, "session/:sessionId/element/:id/element",
          WrapToCommand("FindChildElement",
                        base::BindRepeating(&ExecuteFindChildElement, 50))),
      CommandMapping(
          kPost, "session/:sessionId/element/:id/elements",
          WrapToCommand("FindChildElements",
                        base::BindRepeating(&ExecuteFindChildElements, 50))),
      CommandMapping(
          kGet, "session/:sessionId/element/:id/selected",
          WrapToCommand("IsElementSelected",
                        base::BindRepeating(&ExecuteIsElementSelected))),
      CommandMapping(
          kGet, "session/:sessionId/element/:id/attribute/:name",
          WrapToCommand("GetElementAttribute",
                        base::BindRepeating(&ExecuteGetElementAttribute))),
      CommandMapping(
          kGet, "session/:sessionId/element/:id/property/:name",
          WrapToCommand("GetElementProperty",
                        base::BindRepeating(&ExecuteGetElementProperty))),
      CommandMapping(kGet, "session/:sessionId/element/:id/css/:propertyName",
                     WrapToCommand("GetElementCSSProperty",
                                   base::BindRepeating(
                                       &ExecuteGetElementValueOfCSSProperty))),
      CommandMapping(
          kGet, "session/:sessionId/element/:id/text",
          WrapToCommand("GetElementText",
                        base::BindRepeating(&ExecuteGetElementText))),
      CommandMapping(
          kGet, "session/:sessionId/element/:id/name",
          WrapToCommand("GetElementTagName",
                        base::BindRepeating(&ExecuteGetElementTagName))),
      CommandMapping(
          kGet, "session/:sessionId/element/:id/rect",
          WrapToCommand("GetElementRect",
                        base::BindRepeating(&ExecuteGetElementRect))),
      CommandMapping(
          kGet, "session/:sessionId/element/:id/enabled",
          WrapToCommand("IsElementEnabled",
                        base::BindRepeating(&ExecuteIsElementEnabled))),
      CommandMapping(kPost, "session/:sessionId/element/:id/click",
                     WrapToCommand("ClickElement",
                                   base::BindRepeating(&ExecuteClickElement))),
      CommandMapping(kPost, "session/:sessionId/element/:id/clear",
                     WrapToCommand("ClearElement",
                                   base::BindRepeating(&ExecuteClearElement))),

      CommandMapping(
          kPost, "session/:sessionId/element/:id/value",
          WrapToCommand("TypeElement",
                        base::BindRepeating(&ExecuteSendKeysToElement))),

      CommandMapping(kGet, "session/:sessionId/source",
                     WrapToCommand("GetSource",
                                   base::BindRepeating(&ExecuteGetPageSource))),
      CommandMapping(kPost, "session/:sessionId/execute/sync",
                     WrapToCommand("ExecuteScript",
                                   base::BindRepeating(&ExecuteExecuteScript))),
      CommandMapping(
          kPost, "session/:sessionId/execute/async",
          WrapToCommand("ExecuteAsyncScript",
                        base::BindRepeating(&ExecuteExecuteAsyncScript))),

      CommandMapping(
          kGet, "session/:sessionId/cookie",
          WrapToCommand("GetCookies", base::BindRepeating(&ExecuteGetCookies))),
      CommandMapping(
          kGet, "session/:sessionId/cookie/:name",
          WrapToCommand("GetNamedCookie",
                        base::BindRepeating(&ExecuteGetNamedCookie))),
      CommandMapping(
          kPost, "session/:sessionId/cookie",
          WrapToCommand("AddCookie", base::BindRepeating(&ExecuteAddCookie))),
      CommandMapping(kDelete, "session/:sessionId/cookie/:name",
                     WrapToCommand("DeleteCookie",
                                   base::BindRepeating(&ExecuteDeleteCookie))),
      CommandMapping(
          kDelete, "session/:sessionId/cookie",
          WrapToCommand("DeleteAllCookies",
                        base::BindRepeating(&ExecuteDeleteAllCookies))),

      CommandMapping(
          kPost, "session/:sessionId/actions",
          WrapToCommand("PerformActions",
                        base::BindRepeating(&ExecutePerformActions))),
      CommandMapping(
          kDelete, "session/:sessionId/actions",
          WrapToCommand("ReleaseActions",
                        base::BindRepeating(&ExecuteReleaseActions))),

      CommandMapping(
          kPost, "session/:sessionId/alert/dismiss",
          WrapToCommand(
              "DismissAlert",
              base::BindRepeating(&ExecuteAlertCommand,
                                  base::BindRepeating(&ExecuteDismissAlert)))),
      CommandMapping(
          kPost, "session/:sessionId/alert/accept",
          WrapToCommand(
              "AcceptAlert",
              base::BindRepeating(&ExecuteAlertCommand,
                                  base::BindRepeating(&ExecuteAcceptAlert)))),
      CommandMapping(
          kGet, "session/:sessionId/alert/text",
          WrapToCommand(
              "GetAlertMessage",
              base::BindRepeating(&ExecuteAlertCommand,
                                  base::BindRepeating(&ExecuteGetAlertText)))),
      CommandMapping(
          kPost, "session/:sessionId/alert/text",
          WrapToCommand(
              "SetAlertPrompt",
              base::BindRepeating(&ExecuteAlertCommand,
                                  base::BindRepeating(&ExecuteSetAlertText)))),

      CommandMapping(
          kGet, "session/:sessionId/screenshot",
          WrapToCommand("Screenshot", base::BindRepeating(&ExecuteScreenshot))),

      CommandMapping(
          kGet, "session/:sessionId/element/:id/screenshot",
          WrapToCommand("ElementScreenshot",
                        base::BindRepeating(&ExecuteElementScreenshot))),

      //
      // Json wire protocol endpoints
      //

      // No W3C equivalent.
      CommandMapping(
          kGet, "sessions",
          base::BindRepeating(
              &ExecuteGetSessions,
              WrapToCommand("GetSessions", base::BindRepeating(
                                               &ExecuteGetSessionCapabilities)),
              &session_thread_map_)),

      // No W3C equivalent.
      CommandMapping(
          kGet, "session/:sessionId",
          WrapToCommand("GetSessionCapabilities",
                        base::BindRepeating(&ExecuteGetSessionCapabilities),
                        false /*w3c_standard_command*/)),

      // Subset of W3C POST /session/:sessionId/timeouts.
      CommandMapping(kPost, "session/:sessionId/timeouts/implicit_wait",
                     WrapToCommand("SetImplicitWait",
                                   base::BindRepeating(&ExecuteImplicitlyWait),
                                   false /*w3c_standard_command*/)),

      // Subset of W3C POST /session/:sessionId/timeouts.
      CommandMapping(
          kPost, "session/:sessionId/timeouts/async_script",
          WrapToCommand("SetScriptTimeout",
                        base::BindRepeating(&ExecuteSetScriptTimeout),
                        false /*w3c_standard_command*/)),

      // Similar to W3C GET /session/:sessionId/window.
      CommandMapping(
          kGet, "session/:sessionId/window_handle",
          WrapToCommand("GetWindow",
                        base::BindRepeating(&ExecuteGetCurrentWindowHandle),
                        false /*w3c_standard_command*/)),

      // Similar to W3C GET /session/:sessionId/window/handles
      CommandMapping(
          kGet, "session/:sessionId/window_handles",
          WrapToCommand("GetWindows",
                        base::BindRepeating(&ExecuteGetWindowHandles),
                        false /*w3c_standard_command*/)),

      // Similar to W3C POST /session/:sessionId/execute/sync.
      CommandMapping(kPost, "session/:sessionId/execute",
                     WrapToCommand("ExecuteScript",
                                   base::BindRepeating(&ExecuteExecuteScript),
                                   false /*w3c_standard_command*/)),

      // Similar to W3C POST /session/:sessionId/execute/async.
      CommandMapping(
          kPost, "session/:sessionId/execute_async",
          WrapToCommand("ExecuteAsyncScript",
                        base::BindRepeating(&ExecuteExecuteAsyncScript),
                        false /*w3c_standard_command*/)),

      // Subset of W3C POST /session/:sessionId/window/rect.
      CommandMapping(kPost, "session/:sessionId/window/:windowHandle/size",
                     WrapToCommand("SetWindowSize",
                                   base::BindRepeating(&ExecuteSetWindowSize),
                                   false /*w3c_standard_command*/)),

      // Subset of W3C GET /session/:sessionId/window/rect.
      CommandMapping(kGet, "session/:sessionId/window/:windowHandle/size",
                     WrapToCommand("GetWindowSize",
                                   base::BindRepeating(&ExecuteGetWindowSize),
                                   false /*w3c_standard_command*/)),

      // Subset of W3C POST /session/:sessionId/window/rect.
      CommandMapping(
          kPost, "session/:sessionId/window/:windowHandle/position",
          WrapToCommand("SetWindowPosition",
                        base::BindRepeating(&ExecuteSetWindowPosition),
                        false /*w3c_standard_command*/)),

      // Subset of W3C GET /session/:sessionId/window/rect.
      CommandMapping(
          kGet, "session/:sessionId/window/:windowHandle/position",
          WrapToCommand("GetWindowPosition",
                        base::BindRepeating(&ExecuteGetWindowPosition),
                        false /*w3c_standard_command*/)),

      // Similar to W3C POST /session/:sessionId/window/maximize.
      CommandMapping(kPost, "session/:sessionId/window/:windowHandle/maximize",
                     WrapToCommand("MaximizeWindow",
                                   base::BindRepeating(&ExecuteMaximizeWindow),
                                   false /*w3c_standard_command*/)),

      // Similar to W3C GET /session/:sessionId/element/active, but is POST.
      CommandMapping(
          kPost, "session/:sessionId/element/active",
          WrapToCommand("GetActiveElement",
                        base::BindRepeating(&ExecuteGetActiveElement),
                        false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(kPost, "session/:sessionId/element/:id/submit",
                     WrapToCommand("SubmitElement",
                                   base::BindRepeating(&ExecuteSubmitElement),
                                   false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(
          kPost, "session/:sessionId/keys",
          WrapToCommand("Type",
                        base::BindRepeating(&ExecuteSendKeysToActiveElement),
                        false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/element/:id/equals/:other",
                     WrapToCommand("IsElementEqual",
                                   base::BindRepeating(&ExecuteElementEquals),
                                   false /*w3c_standard_command*/)),

      // No W3C equivalent. Allowed in W3C mode due to active usage by some APIs
      // and the difficulty for clients to provide an equivalent implementation.
      // This endpoint is mentioned in an appendix of W3C spec
      // (https://www.w3.org/TR/webdriver/#element-displayedness).
      CommandMapping(
          kGet, "session/:sessionId/element/:id/displayed",
          WrapToCommand("IsElementDisplayed",
                        base::BindRepeating(&ExecuteIsElementDisplayed))),

      // No W3C equivalent.
      CommandMapping(
          kGet, "session/:sessionId/element/:id/location",
          WrapToCommand("GetElementLocation",
                        base::BindRepeating(&ExecuteGetElementLocation),
                        false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(
          kGet, "session/:sessionId/element/:id/location_in_view",
          WrapToCommand("GetElementLocationInView",
                        base::BindRepeating(
                            &ExecuteGetElementLocationOnceScrolledIntoView),
                        false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/element/:id/size",
                     WrapToCommand("GetElementSize",
                                   base::BindRepeating(&ExecuteGetElementSize),
                                   false /*w3c_standard_command*/)),

      // Similar to W3C GET /session/:sessionId/alert/text.
      CommandMapping(
          kGet, "session/:sessionId/alert_text",
          WrapToCommand(
              "GetAlertMessage",
              base::BindRepeating(&ExecuteAlertCommand,
                                  base::BindRepeating(&ExecuteGetAlertText)),
              false /*w3c_standard_command*/)),

      // Similar to W3C POST /session/:sessionId/alert/text.
      CommandMapping(
          kPost, "session/:sessionId/alert_text",
          WrapToCommand(
              "SetAlertPrompt",
              base::BindRepeating(&ExecuteAlertCommand,
                                  base::BindRepeating(&ExecuteSetAlertText)),
              false /*w3c_standard_command*/)),

      // Similar to W3C POST /session/:sessionId/alert/accept.
      CommandMapping(
          kPost, "session/:sessionId/accept_alert",
          WrapToCommand(
              "AcceptAlert",
              base::BindRepeating(&ExecuteAlertCommand,
                                  base::BindRepeating(&ExecuteAcceptAlert)),
              false /*w3c_standard_command*/)),

      // Similar to W3C POST /session/:sessionId/alert/dismiss.
      CommandMapping(
          kPost, "session/:sessionId/dismiss_alert",
          WrapToCommand(
              "DismissAlert",
              base::BindRepeating(&ExecuteAlertCommand,
                                  base::BindRepeating(&ExecuteDismissAlert)),
              false /*w3c_standard_command*/)),

      // The following set of commands form a subset of W3C Actions API.
      CommandMapping(
          kPost, "session/:sessionId/moveto",
          WrapToCommand("MouseMove", base::BindRepeating(&ExecuteMouseMoveTo),
                        false /*w3c_standard_command*/)),
      CommandMapping(
          kPost, "session/:sessionId/click",
          WrapToCommand("Click", base::BindRepeating(&ExecuteMouseClick))),
      CommandMapping(kPost, "session/:sessionId/buttondown",
                     WrapToCommand("MouseDown",
                                   base::BindRepeating(&ExecuteMouseButtonDown),
                                   false /*w3c_standard_command*/)),
      CommandMapping(
          kPost, "session/:sessionId/buttonup",
          WrapToCommand("MouseUp", base::BindRepeating(&ExecuteMouseButtonUp),
                        false /*w3c_standard_command*/)),
      CommandMapping(
          kPost, "session/:sessionId/doubleclick",
          WrapToCommand("DoubleClick",
                        base::BindRepeating(&ExecuteMouseDoubleClick),
                        false /*w3c_standard_command*/)),
      CommandMapping(
          kPost, "session/:sessionId/touch/click",
          WrapToCommand("Tap", base::BindRepeating(&ExecuteTouchSingleTap),
                        false /*w3c_standard_command*/)),
      CommandMapping(
          kPost, "session/:sessionId/touch/down",
          WrapToCommand("TouchDown", base::BindRepeating(&ExecuteTouchDown),
                        false /*w3c_standard_command*/)),
      CommandMapping(
          kPost, "session/:sessionId/touch/up",
          WrapToCommand("TouchUp", base::BindRepeating(&ExecuteTouchUp),
                        false /*w3c_standard_command*/)),
      CommandMapping(
          kPost, "session/:sessionId/touch/move",
          WrapToCommand("TouchMove", base::BindRepeating(&ExecuteTouchMove),
                        false /*w3c_standard_command*/)),
      CommandMapping(
          kPost, "session/:sessionId/touch/scroll",
          WrapToCommand("TouchScroll", base::BindRepeating(&ExecuteTouchScroll),
                        false /*w3c_standard_command*/)),
      CommandMapping(kPost, "session/:sessionId/touch/doubleclick",
                     WrapToCommand("TouchDoubleTap",
                                   base::BindRepeating(&ExecuteTouchDoubleTap),
                                   false /*w3c_standard_command*/)),
      CommandMapping(kPost, "session/:sessionId/touch/longclick",
                     WrapToCommand("TouchLongPress",
                                   base::BindRepeating(&ExecuteTouchLongPress),
                                   false /*w3c_standard_command*/)),
      CommandMapping(
          kPost, "session/:sessionId/touch/flick",
          WrapToCommand("TouchFlick", base::BindRepeating(&ExecuteFlick),
                        false /*w3c_standard_command*/)),

      // No W3C equivalent, see .https://crbug.com/chromedriver/3180
      CommandMapping(kGet, "session/:sessionId/location",
                     WrapToCommand("GetGeolocation",
                                   base::BindRepeating(&ExecuteGetLocation))),

      // No W3C equivalent, see .https://crbug.com/chromedriver/3180
      CommandMapping(kPost, "session/:sessionId/location",
                     WrapToCommand("SetGeolocation",
                                   base::BindRepeating(&ExecuteSetLocation))),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/local_storage",
                     WrapToCommand("GetLocalStorageKeys",
                                   base::BindRepeating(&ExecuteGetStorageKeys,
                                                       kLocalStorage),
                                   false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(kPost, "session/:sessionId/local_storage",
                     WrapToCommand("SetLocalStorageKeys",
                                   base::BindRepeating(&ExecuteSetStorageItem,
                                                       kLocalStorage),
                                   false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(kDelete, "session/:sessionId/local_storage",
                     WrapToCommand("ClearLocalStorage",
                                   base::BindRepeating(&ExecuteClearStorage,
                                                       kLocalStorage),
                                   false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/local_storage/key/:key",
                     WrapToCommand("GetLocalStorageItem",
                                   base::BindRepeating(&ExecuteGetStorageItem,
                                                       kLocalStorage),
                                   false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(
          kDelete, "session/:sessionId/local_storage/key/:key",
          WrapToCommand(
              "RemoveLocalStorageItem",
              base::BindRepeating(&ExecuteRemoveStorageItem, kLocalStorage),
              false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/local_storage/size",
                     WrapToCommand("GetLocalStorageSize",
                                   base::BindRepeating(&ExecuteGetStorageSize,
                                                       kLocalStorage),
                                   false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/session_storage",
                     WrapToCommand("GetSessionStorageKeys",
                                   base::BindRepeating(&ExecuteGetStorageKeys,
                                                       kSessionStorage),
                                   false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(kPost, "session/:sessionId/session_storage",
                     WrapToCommand("SetSessionStorageItem",
                                   base::BindRepeating(&ExecuteSetStorageItem,
                                                       kSessionStorage),
                                   false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(kDelete, "session/:sessionId/session_storage",
                     WrapToCommand("ClearSessionStorage",
                                   base::BindRepeating(&ExecuteClearStorage,
                                                       kSessionStorage),
                                   false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/session_storage/key/:key",
                     WrapToCommand("GetSessionStorageItem",
                                   base::BindRepeating(&ExecuteGetStorageItem,
                                                       kSessionStorage),
                                   false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(
          kDelete, "session/:sessionId/session_storage/key/:key",
          WrapToCommand(
              "RemoveSessionStorageItem",
              base::BindRepeating(&ExecuteRemoveStorageItem, kSessionStorage),
              false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/session_storage/size",
                     WrapToCommand("GetSessionStorageSize",
                                   base::BindRepeating(&ExecuteGetStorageSize,
                                                       kSessionStorage),
                                   false /*w3c_standard_command*/)),

      // No W3C equivalent, temporarily enabled until clients start using
      // "session/:sessionId/se/log".
      // Superseded by "session/:sessionId/se/log".
      CommandMapping(
          kPost, "session/:sessionId/log",
          WrapToCommand("GetLog", base::BindRepeating(&ExecuteGetLog))),

      // No W3C equivalent. Superseded by "session/:sessionId/se/log/types".
      CommandMapping(
          kGet, "session/:sessionId/log/types",
          WrapToCommand("GetLogTypes",
                        base::BindRepeating(&ExecuteGetAvailableLogTypes),
                        false /*w3c_standard_command*/)),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/application_cache/status",
                     base::BindRepeating(&ExecuteGetStatus)),

      //
      // Extension commands from other specs.
      //

      // Extension for Reporting API:
      // https://w3c.github.io/reporting/#generate-test-report-command
      CommandMapping(
          kPost, "session/:sessionId/reporting/generate_test_report",
          WrapToCommand("GenerateTestReport",
                        base::BindRepeating(&ExecuteGenerateTestReport))),

      // Extensions from Mobile JSON Wire Protocol:
      // https://github.com/SeleniumHQ/mobile-spec/blob/master/spec-draft.md
      CommandMapping(
          kGet, "session/:sessionId/network_connection",
          WrapToCommand("GetNetworkConnection",
                        base::BindRepeating(&ExecuteGetNetworkConnection))),
      CommandMapping(
          kPost, "session/:sessionId/network_connection",
          WrapToCommand("SetNetworkConnection",
                        base::BindRepeating(&ExecuteSetNetworkConnection))),

      // Extension for WebAuthn API:
      // https://w3c.github.io/webauthn/#sctn-automation
      CommandMapping(kPost, "session/:sessionId/webauthn/authenticator",
                     WrapToCommand("AddVirtualAuthenticator",
                                   base::BindRepeating(
                                       &ExecuteWebAuthnCommand,
                                       base::BindRepeating(
                                           &ExecuteAddVirtualAuthenticator)))),
      CommandMapping(
          kDelete, "session/:sessionId/webauthn/authenticator/:authenticatorId",
          WrapToCommand(
              "RemoveVirtualAuthenticator",
              base::BindRepeating(
                  &ExecuteWebAuthnCommand,
                  base::BindRepeating(&ExecuteRemoveVirtualAuthenticator)))),
      CommandMapping(
          kPost,
          "session/:sessionId/webauthn/authenticator/:authenticatorId/"
          "credential",
          WrapToCommand(
              "AddCredential",
              base::BindRepeating(&ExecuteWebAuthnCommand,
                                  base::BindRepeating(&ExecuteAddCredential)))),
      CommandMapping(
          kGet,
          "session/:sessionId/webauthn/authenticator/:authenticatorId/"
          "credentials",
          WrapToCommand("GetCredentials",
                        base::BindRepeating(
                            &ExecuteWebAuthnCommand,
                            base::BindRepeating(&ExecuteGetCredentials)))),
      CommandMapping(
          kDelete,
          "session/:sessionId/webauthn/authenticator/:authenticatorId/"
          "credentials/:credentialId",
          WrapToCommand("RemoveCredential",
                        base::BindRepeating(
                            &ExecuteWebAuthnCommand,
                            base::BindRepeating(&ExecuteRemoveCredential)))),
      CommandMapping(
          kDelete,
          "session/:sessionId/webauthn/authenticator/:authenticatorId/"
          "credentials",
          WrapToCommand(
              "RemoveAllCredentials",
              base::BindRepeating(
                  &ExecuteWebAuthnCommand,
                  base::BindRepeating(&ExecuteRemoveAllCredentials)))),
      CommandMapping(
          kPost,
          "session/:sessionId/webauthn/authenticator/:authenticatorId/uv",
          WrapToCommand("SetUserVerified",
                        base::BindRepeating(
                            &ExecuteWebAuthnCommand,
                            base::BindRepeating(&ExecuteSetUserVerified)))),

      // Extension for Permissions Standard Automation "set permission" command:
      // https://w3c.github.io/permissions/#set-permission-command
      CommandMapping(kPost, "session/:sessionId/permissions",
                     WrapToCommand("SetPermission",
                                   base::BindRepeating(&ExecuteSetPermission))),

      //
      // Non-standard extension commands
      //

      // Commands to access Chrome logs, defined by agreement with Selenium.
      // Using "se" prefix for "Selenium".
      CommandMapping(
          kPost, "session/:sessionId/se/log",
          WrapToCommand("GetLog", base::BindRepeating(&ExecuteGetLog))),
      CommandMapping(
          kGet, "session/:sessionId/se/log/types",
          WrapToCommand("GetLogTypes",
                        base::BindRepeating(&ExecuteGetAvailableLogTypes))),
      // Command is used by Selenium Java tests
      CommandMapping(
          kPost, "session/:sessionId/file",
          WrapToCommand("UploadFile", base::BindRepeating(&ExecuteUploadFile))),
      // Command is used by Ruby OSS mode
      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/element/:id/value",
                     WrapToCommand("GetElementValue",
                                   base::BindRepeating(&ExecuteGetElementValue),
                                   false /*w3c_standard_command*/)),
      // Command is used by Selenium Java tests
      CommandMapping(
          kGet, kShutdownPath,
          base::BindRepeating(
              &ExecuteQuitAll,
              WrapToCommand("QuitAll", base::BindRepeating(&ExecuteQuit, true)),
              &session_thread_map_)),
      // Command is used by Selenium Java tests
      CommandMapping(
          kPost, kShutdownPath,
          base::BindRepeating(
              &ExecuteQuitAll,
              WrapToCommand("QuitAll", base::BindRepeating(&ExecuteQuit, true)),
              &session_thread_map_)),

      //
      // ChromeDriver specific extension commands.
      //

      CommandMapping(
          kPost, "session/:sessionId/chromium/launch_app",
          WrapToCommand("LaunchApp", base::BindRepeating(&ExecuteLaunchApp))),
      CommandMapping(
          kGet, "session/:sessionId/chromium/heap_snapshot",
          WrapToCommand("HeapSnapshot",
                        base::BindRepeating(&ExecuteTakeHeapSnapshot))),
      CommandMapping(
          kGet, "session/:sessionId/chromium/network_conditions",
          WrapToCommand("GetNetworkConditions",
                        base::BindRepeating(&ExecuteGetNetworkConditions))),
      CommandMapping(
          kPost, "session/:sessionId/chromium/network_conditions",
          WrapToCommand("SetNetworkConditions",
                        base::BindRepeating(&ExecuteSetNetworkConditions))),
      CommandMapping(
          kDelete, "session/:sessionId/chromium/network_conditions",
          WrapToCommand("DeleteNetworkConditions",
                        base::BindRepeating(&ExecuteDeleteNetworkConditions))),
      CommandMapping(kPost, "session/:sessionId/chromium/send_command",
                     WrapToCommand("SendCommand",
                                   base::BindRepeating(&ExecuteSendCommand))),
      VendorPrefixedCommandMapping(
          kPost, "session/:sessionId/%s/cdp/execute",
          WrapToCommand("ExecuteCDP",
                        base::BindRepeating(&ExecuteSendCommandAndGetResult))),
      CommandMapping(
          kPost, "session/:sessionId/chromium/send_command_and_get_result",
          WrapToCommand("SendCommandAndGetResult",
                        base::BindRepeating(&ExecuteSendCommandAndGetResult))),
      VendorPrefixedCommandMapping(
          kPost, "session/:sessionId/%s/page/freeze",
          WrapToCommand("Freeze", base::BindRepeating(&ExecuteFreeze))),
      VendorPrefixedCommandMapping(
          kPost, "session/:sessionId/%s/page/resume",
          WrapToCommand("Resume", base::BindRepeating(&ExecuteResume))),
      VendorPrefixedCommandMapping(
          kPost, "session/:sessionId/%s/cast/set_sink_to_use",
          WrapToCommand("SetSinkToUse",
                        base::BindRepeating(&ExecuteSetSinkToUse))),
      VendorPrefixedCommandMapping(
          kPost, "session/:sessionId/%s/cast/start_tab_mirroring",
          WrapToCommand("StartTabMirroring",
                        base::BindRepeating(&ExecuteStartTabMirroring))),
      VendorPrefixedCommandMapping(
          kPost, "session/:sessionId/%s/cast/stop_casting",
          WrapToCommand("StopCasting",
                        base::BindRepeating(&ExecuteStopCasting))),
      VendorPrefixedCommandMapping(
          kGet, "session/:sessionId/%s/cast/get_sinks",
          WrapToCommand("GetSinks", base::BindRepeating(&ExecuteGetSinks))),
      VendorPrefixedCommandMapping(
          kGet, "session/:sessionId/%s/cast/get_issue_message",
          WrapToCommand("GetIssueMessage",
                        base::BindRepeating(&ExecuteGetIssueMessage))),

      //
      // Commands used for internal testing only.
      // They are used in run_py_tests.py
      //

      CommandMapping(kGet, "session/:sessionId/alert",
                     WrapToCommand("IsAlertOpen",
                                   base::BindRepeating(
                                       &ExecuteAlertCommand,
                                       base::BindRepeating(&ExecuteGetAlert)))),
      CommandMapping(
          kGet, "session/:sessionId/is_loading",
          WrapToCommand("IsLoading", base::BindRepeating(&ExecuteIsLoading))),

      //
      // Special commands used by internal implementation
      // Client apps should never use this over a normal
      // WebDriver http connection
      //

      CommandMapping(
          kPost, kSendCommandFromWebSocket,
          WrapToCommand("SendCommandFromWebSocket",
                        base::BindRepeating(&ExecuteSendCommandFromWebSocket))),
  };
  command_map_.reset(new CommandMap(commands, commands + base::size(commands)));
}

HttpHandler::~HttpHandler() {}

void HttpHandler::Handle(const net::HttpServerRequestInfo& request,
                         const HttpResponseSenderFunc& send_response_func) {
  CHECK(thread_checker_.CalledOnValidThread());

  if (received_shutdown_)
    return;

  std::string path = request.path;
  if (!base::StartsWith(path, url_base_, base::CompareCase::SENSITIVE)) {
    std::unique_ptr<net::HttpServerResponseInfo> response(
        new net::HttpServerResponseInfo(net::HTTP_BAD_REQUEST));
    response->SetBody("unhandled request", "text/plain");
    send_response_func.Run(std::move(response));
    return;
  }

  path.erase(0, url_base_.length());

  HandleCommand(request, path, send_response_func);

  if (path == kShutdownPath)
    received_shutdown_ = true;
}

Command HttpHandler::WrapToCommand(const char* name,
                                   const SessionCommand& session_command,
                                   bool w3c_standard_command) {
  return base::Bind(&ExecuteSessionCommand, &session_thread_map_, name,
                    session_command, w3c_standard_command, false);
}

Command HttpHandler::WrapToCommand(const char* name,
                                   const WindowCommand& window_command,
                                   bool w3c_standard_command) {
  return WrapToCommand(name, base::Bind(&ExecuteWindowCommand, window_command),
                       w3c_standard_command);
}

Command HttpHandler::WrapToCommand(const char* name,
                                   const ElementCommand& element_command,
                                   bool w3c_standard_command) {
  return WrapToCommand(name,
                       base::Bind(&ExecuteElementCommand, element_command),
                       w3c_standard_command);
}

void HttpHandler::HandleCommand(
    const net::HttpServerRequestInfo& request,
    const std::string& trimmed_path,
    const HttpResponseSenderFunc& send_response_func) {
  base::DictionaryValue params;
  std::string session_id;
  CommandMap::const_iterator iter = command_map_->begin();
  while (true) {
    if (iter == command_map_->end()) {
      if (w3cMode(session_id, session_thread_map_)) {
        PrepareResponse(
            trimmed_path, send_response_func,
            Status(kUnknownCommand, "unknown command: " + trimmed_path),
            nullptr, session_id, true);
      } else {
        std::unique_ptr<net::HttpServerResponseInfo> response(
            new net::HttpServerResponseInfo(net::HTTP_NOT_FOUND));
        response->SetBody("unknown command: " + trimmed_path, "text/plain");
        send_response_func.Run(std::move(response));
      }
      return;
    }
    if (internal::MatchesCommand(
            request.method, trimmed_path, *iter, &session_id, &params)) {
      break;
    }
    ++iter;
  }

  if (request.data.length()) {
    base::DictionaryValue* body_params;
    std::unique_ptr<base::Value> parsed_body =
        base::JSONReader::ReadDeprecated(request.data);
    if (!parsed_body || !parsed_body->GetAsDictionary(&body_params)) {
      if (w3cMode(session_id, session_thread_map_)) {
        PrepareResponse(trimmed_path, send_response_func,
                        Status(kInvalidArgument, "missing command parameters"),
                        nullptr, session_id, true);
      } else {
        std::unique_ptr<net::HttpServerResponseInfo> response(
            new net::HttpServerResponseInfo(net::HTTP_BAD_REQUEST));
        response->SetBody("missing command parameters", "text/plain");
        send_response_func.Run(std::move(response));
      }
      return;
    }
    params.MergeDictionary(body_params);
  } else if (iter->method == kPost &&
             w3cMode(session_id, session_thread_map_)) {
    // Data in JSON format is required for POST requests. See step 5 of
    // https://www.w3.org/TR/2018/REC-webdriver1-20180605/#processing-model.
    PrepareResponse(trimmed_path, send_response_func,
                    Status(kInvalidArgument, "missing command parameters"),
                    nullptr, session_id, true);
    return;
  }

  iter->command.Run(params,
                    session_id,
                    base::Bind(&HttpHandler::PrepareResponse,
                               weak_ptr_factory_.GetWeakPtr(),
                               trimmed_path,
                               send_response_func));
}

void HttpHandler::PrepareResponse(
    const std::string& trimmed_path,
    const HttpResponseSenderFunc& send_response_func,
    const Status& status,
    std::unique_ptr<base::Value> value,
    const std::string& session_id,
    bool w3c_compliant) {
  CHECK(thread_checker_.CalledOnValidThread());
  std::unique_ptr<net::HttpServerResponseInfo> response;
  if (w3c_compliant)
    response = PrepareStandardResponse(
        trimmed_path, status, std::move(value), session_id);
  else
    response = PrepareLegacyResponse(trimmed_path,
                                     status,
                                     std::move(value),
                                     session_id);
  send_response_func.Run(std::move(response));
  if (trimmed_path == kShutdownPath)
    quit_func_.Run();
}

std::unique_ptr<net::HttpServerResponseInfo> HttpHandler::PrepareLegacyResponse(
    const std::string& trimmed_path,
    const Status& status,
    std::unique_ptr<base::Value> value,
    const std::string& session_id) {
  if (status.code() == kUnknownCommand) {
    std::unique_ptr<net::HttpServerResponseInfo> response(
        new net::HttpServerResponseInfo(net::HTTP_NOT_IMPLEMENTED));
    response->SetBody("unimplemented command: " + trimmed_path, "text/plain");
    return response;
  }

  if (status.IsError()) {
    Status full_status(status);
    full_status.AddDetails(base::StringPrintf(
        "Driver info: %s=%s,platform=%s %s %s",
        base::ToLowerASCII(kChromeDriverProductShortName).c_str(),
        kChromeDriverVersion, base::SysInfo::OperatingSystemName().c_str(),
        base::SysInfo::OperatingSystemVersion().c_str(),
        base::SysInfo::OperatingSystemArchitecture().c_str()));
    std::unique_ptr<base::DictionaryValue> error(new base::DictionaryValue());
    error->SetString("message", full_status.message());
    value = std::move(error);
  }
  if (!value)
    value = std::make_unique<base::Value>();

  base::DictionaryValue body_params;
  body_params.SetInteger("status", status.code());
  body_params.Set("value", std::move(value));
  body_params.SetString("sessionId", session_id);
  std::string body;
  base::JSONWriter::WriteWithOptions(
      body_params, base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION,
      &body);
  std::unique_ptr<net::HttpServerResponseInfo> response(
      new net::HttpServerResponseInfo(net::HTTP_OK));
  response->SetBody(body, "application/json; charset=utf-8");
  return response;
}

std::unique_ptr<net::HttpServerResponseInfo>
HttpHandler::PrepareStandardResponse(
    const std::string& trimmed_path,
    const Status& status,
    std::unique_ptr<base::Value> value,
    const std::string& session_id) {
  std::unique_ptr<net::HttpServerResponseInfo> response;
  switch (status.code()) {
    case kOk:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_OK));
      break;
    // error codes
    case kElementClickIntercepted:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_BAD_REQUEST));
      break;
    case kElementNotInteractable:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_BAD_REQUEST));
      break;
    case kInvalidArgument:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_BAD_REQUEST));
      break;
    case kInvalidCookieDomain:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_BAD_REQUEST));
      break;
    case kInvalidElementState:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_BAD_REQUEST));
      break;
    case kInvalidSelector:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_BAD_REQUEST));
      break;
    case kInvalidSessionId:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_NOT_FOUND));
      break;
    case kJavaScriptError:
      response.reset(
          new net::HttpServerResponseInfo(net::HTTP_INTERNAL_SERVER_ERROR));
      break;
    case kMoveTargetOutOfBounds:
      response.reset(
          new net::HttpServerResponseInfo(net::HTTP_INTERNAL_SERVER_ERROR));
      break;
    case kNoSuchAlert:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_NOT_FOUND));
      break;
    case kNoSuchCookie:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_NOT_FOUND));
      break;
    case kNoSuchElement:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_NOT_FOUND));
      break;
    case kNoSuchFrame:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_NOT_FOUND));
      break;
    case kNoSuchWindow:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_NOT_FOUND));
      break;
    case kScriptTimeout:
      response.reset(
          new net::HttpServerResponseInfo(net::HTTP_INTERNAL_SERVER_ERROR));
      break;
    case kSessionNotCreated:
      response.reset(
          new net::HttpServerResponseInfo(net::HTTP_INTERNAL_SERVER_ERROR));
      break;
    case kStaleElementReference:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_NOT_FOUND));
      break;
    case kTimeout:
      response.reset(
          new net::HttpServerResponseInfo(net::HTTP_REQUEST_TIMEOUT));
      break;
    case kUnableToSetCookie:
      response.reset(
          new net::HttpServerResponseInfo(net::HTTP_INTERNAL_SERVER_ERROR));
      break;
    case kUnexpectedAlertOpen:
      response.reset(
          new net::HttpServerResponseInfo(net::HTTP_INTERNAL_SERVER_ERROR));
      break;
    case kUnknownCommand:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_NOT_FOUND));
      break;
    case kUnknownError:
      response.reset(
          new net::HttpServerResponseInfo(net::HTTP_INTERNAL_SERVER_ERROR));
      break;
    case kUnsupportedOperation:
      response.reset(
          new net::HttpServerResponseInfo(net::HTTP_INTERNAL_SERVER_ERROR));
      break;
    case kTargetDetached:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_NOT_FOUND));
      break;

    // TODO(kereliuk): evaluate the usage of these as they relate to the spec
    case kElementNotVisible:
    case kXPathLookupError:
    case kNoSuchExecutionContext:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_BAD_REQUEST));
      break;
    case kChromeNotReachable:
    case kDisconnected:
    case kForbidden:
    case kTabCrashed:
      response.reset(
          new net::HttpServerResponseInfo(net::HTTP_INTERNAL_SERVER_ERROR));
      break;

    default:
      DCHECK(false);
      response.reset(
          new net::HttpServerResponseInfo(net::HTTP_INTERNAL_SERVER_ERROR));
      break;
  }

  if (!value)
    value = std::make_unique<base::Value>();

  base::DictionaryValue body_params;
  if (status.IsError()){
    std::unique_ptr<base::DictionaryValue> inner_params(
        new base::DictionaryValue());
    inner_params->SetString("error", StatusCodeToString(status.code()));
    inner_params->SetString("message", status.message());
    inner_params->SetString("stacktrace", status.stack_trace());
    // According to
    // https://www.w3.org/TR/2018/REC-webdriver1-20180605/#dfn-annotated-unexpected-alert-open-error
    // error UnexpectedAlertOpen should contain 'data.text' with alert text
    if (status.code() == kUnexpectedAlertOpen) {
      const std::string& message = status.message();
      auto first = message.find("{");
      auto last = message.find_last_of("}");
      if (first == std::string::npos || last == std::string::npos) {
        inner_params->SetString("data.text", "");
      } else {
        std::string alertText = message.substr(first, last - first);
        alertText = alertText.substr(alertText.find(":") + 2);
        inner_params->SetString("data.text", alertText);
      }
    }
    body_params.SetDictionary("value", std::move(inner_params));
  } else {
    body_params.Set("value", std::move(value));
  }

  std::string body;
  base::JSONWriter::WriteWithOptions(
      body_params, base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION,
      &body);
  response->SetBody(body, "application/json; charset=utf-8");
  response->AddHeader("cache-control", "no-cache");
  return response;
}


namespace internal {

const char kNewSessionPathPattern[] = "session";

bool MatchesMethod(HttpMethod command_method, const std::string& method) {
  std::string lower_method = base::ToLowerASCII(method);
  switch (command_method) {
    case kGet:
      return lower_method == "get";
    case kPost:
      return lower_method == "post" || lower_method == "put";
    case kDelete:
      return lower_method == "delete";
  }
  return false;
}

bool MatchesCommand(const std::string& method,
                    const std::string& path,
                    const CommandMapping& command,
                    std::string* session_id,
                    base::DictionaryValue* out_params) {
  if (!MatchesMethod(command.method, method))
    return false;

  std::vector<std::string> path_parts = base::SplitString(
      path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::vector<std::string> command_path_parts = base::SplitString(
      command.path_pattern, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (path_parts.size() != command_path_parts.size())
    return false;

  base::DictionaryValue params;
  for (size_t i = 0; i < path_parts.size(); ++i) {
    CHECK(command_path_parts[i].length());
    if (command_path_parts[i][0] == ':') {
      std::string name = command_path_parts[i];
      name.erase(0, 1);
      CHECK(name.length());
      url::RawCanonOutputT<base::char16> output;
      url::DecodeURLEscapeSequences(
          path_parts[i].data(), path_parts[i].length(),
          url::DecodeURLMode::kUTF8OrIsomorphic, &output);
      std::string decoded = base::UTF16ToASCII(
          base::string16(output.data(), output.length()));
      // Due to crbug.com/533361, the url decoding libraries decodes all of the
      // % escape sequences except for %%. We need to handle this case manually.
      // So, replacing all the instances of "%%" with "%".
      base::ReplaceSubstringsAfterOffset(&decoded, 0 , "%%" , "%");
      if (name == "sessionId")
        *session_id = decoded;
      else
        params.SetString(name, decoded);
    } else if (command_path_parts[i] != path_parts[i]) {
      return false;
    }
  }
  out_params->MergeDictionary(&params);
  return true;
}

}  // namespace internal
