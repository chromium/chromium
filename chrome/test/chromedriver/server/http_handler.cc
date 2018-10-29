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
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/alert_commands.h"
#include "chrome/test/chromedriver/chrome/adb_impl.h"
#include "chrome/test/chromedriver/chrome/device_manager.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/session_thread_map.h"
#include "chrome/test/chromedriver/util.h"
#include "chrome/test/chromedriver/version.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/transitional_url_loader_factory_owner.h"
#include "url/url_util.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace {

const char kLocalStorage[] = "localStorage";
const char kSessionStorage[] = "sessionStorage";
const char kShutdownPath[] = "shutdown";

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

  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
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
  void Clone(network::mojom::URLLoaderFactoryRequest factory) override {
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

HttpHandler::HttpHandler(const std::string& url_base)
    : url_base_(url_base),
      received_shutdown_(false),
      command_map_(new CommandMap()),
      weak_ptr_factory_(this) {}

HttpHandler::HttpHandler(
    const base::Closure& quit_func,
    const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const std::string& url_base,
    int adb_port)
    : quit_func_(quit_func),
      url_base_(url_base),
      received_shutdown_(false),
      weak_ptr_factory_(this) {
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
                         base::BindRepeating(&ExecuteQuit, false), true)),
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
                        base::BindRepeating(&ExecuteUnimplementedCommand))),
      CommandMapping(
          kDelete, "session/:sessionId/actions",
          WrapToCommand("DeleteActions",
                        base::BindRepeating(&ExecuteUnimplementedCommand))),

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
                        base::BindRepeating(&ExecuteGetSessionCapabilities))),

      // Subset of W3C POST /session/:sessionId/timeouts.
      CommandMapping(
          kPost, "session/:sessionId/timeouts/implicit_wait",
          WrapToCommand("SetImplicitWait",
                        base::BindRepeating(&ExecuteImplicitlyWait))),

      // Subset of W3C POST /session/:sessionId/timeouts.
      CommandMapping(
          kPost, "session/:sessionId/timeouts/async_script",
          WrapToCommand("SetScriptTimeout",
                        base::BindRepeating(&ExecuteSetScriptTimeout))),

      // Similar to W3C GET /session/:sessionId/window.
      CommandMapping(
          kGet, "session/:sessionId/window_handle",
          WrapToCommand("GetWindow",
                        base::BindRepeating(&ExecuteGetCurrentWindowHandle))),

      // Similar to W3C GET /session/:sessionId/window/handles
      CommandMapping(
          kGet, "session/:sessionId/window_handles",
          WrapToCommand("GetWindows",
                        base::BindRepeating(&ExecuteGetWindowHandles))),

      // Similar to W3C POST /session/:sessionId/execute/sync.
      CommandMapping(kPost, "session/:sessionId/execute",
                     WrapToCommand("ExecuteScript",
                                   base::BindRepeating(&ExecuteExecuteScript))),

      // Similar to W3C POST /session/:sessionId/execute/async.
      CommandMapping(
          kPost, "session/:sessionId/execute_async",
          WrapToCommand("ExecuteAsyncScript",
                        base::BindRepeating(&ExecuteExecuteAsyncScript))),

      // Subset of W3C POST /session/:sessionId/window/rect.
      CommandMapping(kPost, "session/:sessionId/window/:windowHandle/size",
                     WrapToCommand("SetWindowSize",
                                   base::BindRepeating(&ExecuteSetWindowSize))),

      // Subset of W3C GET /session/:sessionId/window/rect.
      CommandMapping(kGet, "session/:sessionId/window/:windowHandle/size",
                     WrapToCommand("GetWindowSize",
                                   base::BindRepeating(&ExecuteGetWindowSize))),

      // Subset of W3C POST /session/:sessionId/window/rect.
      CommandMapping(
          kPost, "session/:sessionId/window/:windowHandle/position",
          WrapToCommand("SetWindowPosition",
                        base::BindRepeating(&ExecuteSetWindowPosition))),

      // Subset of W3C GET /session/:sessionId/window/rect.
      CommandMapping(
          kGet, "session/:sessionId/window/:windowHandle/position",
          WrapToCommand("GetWindowPosition",
                        base::BindRepeating(&ExecuteGetWindowPosition))),

      // Similar to W3C POST /session/:sessionId/window/maximize.
      CommandMapping(
          kPost, "session/:sessionId/window/:windowHandle/maximize",
          WrapToCommand("MaximizeWindow",
                        base::BindRepeating(&ExecuteMaximizeWindow))),

      // Similar to W3C GET /session/:sessionId/element/active, but is POST.
      CommandMapping(
          kPost, "session/:sessionId/element/active",
          WrapToCommand("GetActiveElement",
                        base::BindRepeating(&ExecuteGetActiveElement))),

      // No W3C equivalent.
      CommandMapping(kPost, "session/:sessionId/element/:id/submit",
                     WrapToCommand("SubmitElement",
                                   base::BindRepeating(&ExecuteSubmitElement))),

      // No W3C equivalent.
      CommandMapping(
          kPost, "session/:sessionId/keys",
          WrapToCommand("Type",
                        base::BindRepeating(&ExecuteSendKeysToActiveElement))),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/element/:id/equals/:other",
                     WrapToCommand("IsElementEqual",
                                   base::BindRepeating(&ExecuteElementEquals))),

      // No W3C equivalent.
      CommandMapping(
          kGet, "session/:sessionId/element/:id/displayed",
          WrapToCommand("IsElementDisplayed",
                        base::BindRepeating(&ExecuteIsElementDisplayed))),

      // No W3C equivalent.
      CommandMapping(
          kGet, "session/:sessionId/element/:id/location",
          WrapToCommand("GetElementLocation",
                        base::BindRepeating(&ExecuteGetElementLocation))),

      // No W3C equivalent.
      CommandMapping(
          kGet, "session/:sessionId/element/:id/location_in_view",
          WrapToCommand("GetElementLocationInView",
                        base::BindRepeating(
                            &ExecuteGetElementLocationOnceScrolledIntoView))),

      // No W3C equivalent.
      CommandMapping(
          kGet, "session/:sessionId/element/:id/size",
          WrapToCommand("GetElementSize",
                        base::BindRepeating(&ExecuteGetElementSize))),

      // No W3C equivalent.
      CommandMapping(
          kGet, "session/:sessionId/orientation",
          WrapToCommand("GetScreenOrientation",
                        base::BindRepeating(&ExecuteGetScreenOrientation))),

      // No W3C equivalent.
      CommandMapping(
          kPost, "session/:sessionId/orientation",
          WrapToCommand("SetScreenOrientation",
                        base::BindRepeating(&ExecuteSetScreenOrientation))),

      // Similar to W3C GET /session/:sessionId/alert/text.
      CommandMapping(
          kGet, "session/:sessionId/alert_text",
          WrapToCommand(
              "GetAlertMessage",
              base::BindRepeating(&ExecuteAlertCommand,
                                  base::BindRepeating(&ExecuteGetAlertText)))),

      // Similar to W3C POST /session/:sessionId/alert/text.
      CommandMapping(
          kPost, "session/:sessionId/alert_text",
          WrapToCommand(
              "SetAlertPrompt",
              base::BindRepeating(&ExecuteAlertCommand,
                                  base::BindRepeating(&ExecuteSetAlertText)))),

      // Similar to W3C POST /session/:sessionId/alert/accept.
      CommandMapping(
          kPost, "session/:sessionId/accept_alert",
          WrapToCommand(
              "AcceptAlert",
              base::BindRepeating(&ExecuteAlertCommand,
                                  base::BindRepeating(&ExecuteAcceptAlert)))),

      // Similar to W3C POST /session/:sessionId/alert/dismiss.
      CommandMapping(
          kPost, "session/:sessionId/dismiss_alert",
          WrapToCommand(
              "DismissAlert",
              base::BindRepeating(&ExecuteAlertCommand,
                                  base::BindRepeating(&ExecuteDismissAlert)))),

      // The following set of commands form a subset of W3C Actions API.
      CommandMapping(
          kPost, "session/:sessionId/moveto",
          WrapToCommand("MouseMove", base::BindRepeating(&ExecuteMouseMoveTo))),
      CommandMapping(
          kPost, "session/:sessionId/click",
          WrapToCommand("Click", base::BindRepeating(&ExecuteMouseClick))),
      CommandMapping(kPost, "session/:sessionId/buttondown",
                     WrapToCommand("MouseDown", base::BindRepeating(
                                                    &ExecuteMouseButtonDown))),
      CommandMapping(
          kPost, "session/:sessionId/buttonup",
          WrapToCommand("MouseUp", base::BindRepeating(&ExecuteMouseButtonUp))),
      CommandMapping(
          kPost, "session/:sessionId/doubleclick",
          WrapToCommand("DoubleClick",
                        base::BindRepeating(&ExecuteMouseDoubleClick))),
      CommandMapping(
          kPost, "session/:sessionId/touch/click",
          WrapToCommand("Tap", base::BindRepeating(&ExecuteTouchSingleTap))),
      CommandMapping(
          kPost, "session/:sessionId/touch/down",
          WrapToCommand("TouchDown", base::BindRepeating(&ExecuteTouchDown))),
      CommandMapping(
          kPost, "session/:sessionId/touch/up",
          WrapToCommand("TouchUp", base::BindRepeating(&ExecuteTouchUp))),
      CommandMapping(
          kPost, "session/:sessionId/touch/move",
          WrapToCommand("TouchMove", base::BindRepeating(&ExecuteTouchMove))),
      CommandMapping(kPost, "session/:sessionId/touch/scroll",
                     WrapToCommand("TouchScroll",
                                   base::BindRepeating(&ExecuteTouchScroll))),
      CommandMapping(
          kPost, "session/:sessionId/touch/doubleclick",
          WrapToCommand("TouchDoubleTap",
                        base::BindRepeating(&ExecuteTouchDoubleTap))),
      CommandMapping(
          kPost, "session/:sessionId/touch/longclick",
          WrapToCommand("TouchLongPress",
                        base::BindRepeating(&ExecuteTouchLongPress))),
      CommandMapping(
          kPost, "session/:sessionId/touch/flick",
          WrapToCommand("TouchFlick", base::BindRepeating(&ExecuteFlick))),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/location",
                     WrapToCommand("GetGeolocation",
                                   base::BindRepeating(&ExecuteGetLocation))),

      // No W3C equivalent.
      CommandMapping(kPost, "session/:sessionId/location",
                     WrapToCommand("SetGeolocation",
                                   base::BindRepeating(&ExecuteSetLocation))),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/local_storage",
                     WrapToCommand("GetLocalStorageKeys",
                                   base::BindRepeating(&ExecuteGetStorageKeys,
                                                       kLocalStorage))),

      // No W3C equivalent.
      CommandMapping(kPost, "session/:sessionId/local_storage",
                     WrapToCommand("SetLocalStorageKeys",
                                   base::BindRepeating(&ExecuteSetStorageItem,
                                                       kLocalStorage))),

      // No W3C equivalent.
      CommandMapping(kDelete, "session/:sessionId/local_storage",
                     WrapToCommand("ClearLocalStorage",
                                   base::BindRepeating(&ExecuteClearStorage,
                                                       kLocalStorage))),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/local_storage/key/:key",
                     WrapToCommand("GetLocalStorageItem",
                                   base::BindRepeating(&ExecuteGetStorageItem,
                                                       kLocalStorage))),

      // No W3C equivalent.
      CommandMapping(
          kDelete, "session/:sessionId/local_storage/key/:key",
          WrapToCommand(
              "RemoveLocalStorageItem",
              base::BindRepeating(&ExecuteRemoveStorageItem, kLocalStorage))),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/local_storage/size",
                     WrapToCommand("GetLocalStorageSize",
                                   base::BindRepeating(&ExecuteGetStorageSize,
                                                       kLocalStorage))),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/session_storage",
                     WrapToCommand("GetSessionStorageKeys",
                                   base::BindRepeating(&ExecuteGetStorageKeys,
                                                       kSessionStorage))),

      // No W3C equivalent.
      CommandMapping(kPost, "session/:sessionId/session_storage",
                     WrapToCommand("SetSessionStorageItem",
                                   base::BindRepeating(&ExecuteSetStorageItem,
                                                       kSessionStorage))),

      // No W3C equivalent.
      CommandMapping(kDelete, "session/:sessionId/session_storage",
                     WrapToCommand("ClearSessionStorage",
                                   base::BindRepeating(&ExecuteClearStorage,
                                                       kSessionStorage))),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/session_storage/key/:key",
                     WrapToCommand("GetSessionStorageItem",
                                   base::BindRepeating(&ExecuteGetStorageItem,
                                                       kSessionStorage))),

      // No W3C equivalent.
      CommandMapping(
          kDelete, "session/:sessionId/session_storage/key/:key",
          WrapToCommand(
              "RemoveSessionStorageItem",
              base::BindRepeating(&ExecuteRemoveStorageItem, kSessionStorage))),

      // No W3C equivalent.
      CommandMapping(kGet, "session/:sessionId/session_storage/size",
                     WrapToCommand("GetSessionStorageSize",
                                   base::BindRepeating(&ExecuteGetStorageSize,
                                                       kSessionStorage))),

      // No W3C equivalent.
      CommandMapping(
          kPost, "session/:sessionId/log",
          WrapToCommand("GetLog", base::BindRepeating(&ExecuteGetLog))),

      // No W3C equivalent.
      CommandMapping(
          kGet, "session/:sessionId/log/types",
          WrapToCommand("GetLogTypes",
                        base::BindRepeating(&ExecuteGetAvailableLogTypes))),

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
      CommandMapping(
          kPost, "session/:sessionId/goog/cdp/execute",
          WrapToCommand("ExecuteCDP",
                        base::BindRepeating(&ExecuteSendCommandAndGetResult))),
      CommandMapping(
          kPost, "session/:sessionId/chromium/send_command_and_get_result",
          WrapToCommand("SendCommandAndGetResult",
                        base::BindRepeating(&ExecuteSendCommandAndGetResult))),
      CommandMapping(
          kPost, "session/:sessionId/goog/page/freeze",
          WrapToCommand("Freeze", base::BindRepeating(&ExecuteFreeze))),
      CommandMapping(
          kPost, "session/:sessionId/goog/page/resume",
          WrapToCommand("Resume", base::BindRepeating(&ExecuteResume))),

      //
      // Commands of unknown origins.
      //

      // Similar to W3C POST /session/:sessionId/window/minimize.
      CommandMapping(
          kPost, "session/:sessionId/window/:windowHandle/minimize",
          WrapToCommand("MinimizeWindow",
                        base::BindRepeating(&ExecuteMinimizeWindow))),

      CommandMapping(kGet, "session/:sessionId/alert",
                     WrapToCommand("IsAlertOpen",
                                   base::BindRepeating(
                                       &ExecuteAlertCommand,
                                       base::BindRepeating(&ExecuteGetAlert)))),
      CommandMapping(
          kPost, "session/:sessionId/file",
          WrapToCommand("UploadFile", base::BindRepeating(&ExecuteUploadFile))),
      CommandMapping(
          kGet, "session/:sessionId/element/:id/value",
          WrapToCommand("GetElementValue",
                        base::BindRepeating(&ExecuteGetElementValue))),
      CommandMapping(
          kPost, "session/:sessionId/element/:id/hover",
          WrapToCommand("HoverElement",
                        base::BindRepeating(&ExecuteHoverOverElement))),
      CommandMapping(
          kDelete, "session/:sessionId/orientation",
          WrapToCommand("DeleteScreenOrientation",
                        base::BindRepeating(&ExecuteDeleteScreenOrientation))),
      CommandMapping(
          kGet, kShutdownPath,
          base::BindRepeating(
              &ExecuteQuitAll,
              WrapToCommand("QuitAll", base::BindRepeating(&ExecuteQuit, true)),
              &session_thread_map_)),
      CommandMapping(
          kPost, kShutdownPath,
          base::BindRepeating(
              &ExecuteQuitAll,
              WrapToCommand("QuitAll", base::BindRepeating(&ExecuteQuit, true)),
              &session_thread_map_)),
      CommandMapping(
          kGet, "session/:sessionId/is_loading",
          WrapToCommand("IsLoading", base::BindRepeating(&ExecuteIsLoading))),
      CommandMapping(
          kGet, "session/:sessionId/autoreport",
          WrapToCommand("IsAutoReporting",
                        base::BindRepeating(&ExecuteIsAutoReporting))),
      CommandMapping(
          kPost, "session/:sessionId/autoreport",
          WrapToCommand("SetAutoReporting",
                        base::BindRepeating(&ExecuteSetAutoReporting))),
      CommandMapping(
          kPost, "session/:sessionId/touch/pinch",
          WrapToCommand("TouchPinch", base::BindRepeating(&ExecuteTouchPinch))),
  };
  command_map_.reset(
      new CommandMap(commands, commands + arraysize(commands)));
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

Command HttpHandler::WrapToCommand(
    const char* name,
    const SessionCommand& session_command) {
  return base::Bind(&ExecuteSessionCommand,
                    &session_thread_map_,
                    name,
                    session_command,
                    false);
}

Command HttpHandler::WrapToCommand(
    const char* name,
    const WindowCommand& window_command) {
  return WrapToCommand(name, base::Bind(&ExecuteWindowCommand, window_command));
}

Command HttpHandler::WrapToCommand(
    const char* name,
    const ElementCommand& element_command) {
  return WrapToCommand(name,
                       base::Bind(&ExecuteElementCommand, element_command));
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
      std::unique_ptr<net::HttpServerResponseInfo> response(
          new net::HttpServerResponseInfo(net::HTTP_NOT_FOUND));
      response->SetBody("unknown command: " + trimmed_path, "text/plain");
      send_response_func.Run(std::move(response));
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
        base::JSONReader::Read(request.data);
    if (!parsed_body || !parsed_body->GetAsDictionary(&body_params)) {
      std::unique_ptr<net::HttpServerResponseInfo> response(
          new net::HttpServerResponseInfo(net::HTTP_BAD_REQUEST));
      response->SetBody("missing command parameters", "text/plain");
      send_response_func.Run(std::move(response));
      return;
    }
    params.MergeDictionary(body_params);
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
        "Driver info: chromedriver=%s,platform=%s %s %s",
        kChromeDriverVersion,
        base::SysInfo::OperatingSystemName().c_str(),
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
    case kJavaScriptError:
      response.reset(new net::HttpServerResponseInfo(net::HTTP_BAD_REQUEST));
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
          new net::HttpServerResponseInfo(net::HTTP_REQUEST_TIMEOUT));
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
    case kInvalidSessionId:
    case kChromeNotReachable:
    case kDisconnected:
    case kForbidden:
    case kTabCrashed:
      response.reset(
          new net::HttpServerResponseInfo(net::HTTP_INTERNAL_SERVER_ERROR));
      break;
  }

  if (!value)
    value = std::make_unique<base::Value>();

  base::DictionaryValue body_params;
  if (status.IsError()){
    // Separates status default message from additional details.
    std::vector<std::string> status_details = base::SplitString(
        status.message(), ":\n", base::TRIM_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);
    std::string message;
    for (size_t i=1; i<status_details.size();++i)
      message += status_details[i];
    std::unique_ptr<base::DictionaryValue> inner_params(
        new base::DictionaryValue());
    inner_params->SetString("error", status_details[0]);
    inner_params->SetString("message", message);
    inner_params->SetString("stacktrace", status.stack_trace());
    body_params.SetDictionary("value", std::move(inner_params));
  } else {
    body_params.Set("value", std::move(value));
  }

  std::string body;
  base::JSONWriter::WriteWithOptions(
      body_params, base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION,
      &body);
  response->SetBody(body, "application/json; charset=utf-8");
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
          path_parts[i].data(), path_parts[i].length(), &output);
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
