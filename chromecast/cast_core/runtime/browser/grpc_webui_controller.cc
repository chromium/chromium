// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/grpc_webui_controller.h"

#include "base/command_line.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/browser/webui/cast_webui_message_handler.h"
#include "chromecast/browser/webui/constants.h"
#include "chromecast/cast_core/runtime/browser/grpc_resource_data_source.h"
#include "chromecast/chromecast_buildflags.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/bindings_policy.h"

namespace {

// Javascript callbacks from renderer.
constexpr const char kFuncStartPingNotify[] = "startPingNotify";
constexpr const char kFuncStopPingNotify[] = "stopPingNotify";
constexpr const char kFuncSetOobeFinished[] = "setOobeFinished";
constexpr const char kFuncRecordAction[] = "recordAction";
constexpr const char kFuncLaunchTutorial[] = "launchTutorial";
constexpr const char kFuncGetQRCode[] = "getQRCode";

// Javascript functions called by callbacks.
constexpr const char kJSPingNotifyCallback[] =
    "home_web_ui_.pingNotifyCallback";
constexpr const char kJSEurekaInfoChangedCallback[] =
    "home_web_ui_.eurekaInfoChangedCallback";

// ContentSecurityOverride Prefix.
constexpr const char kContentSecurityPolicyOverride[] =
    "frame-src https://*.google.com,";

}  // namespace

namespace chromecast {

#if !BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
// static
std::unique_ptr<CastCoreWebUI> CastCoreWebUI::Create(
    content::WebUI* webui,
    const std::string host,
    cast::v2::CoreApplicationServiceStub* core_app_service_stub) {
  return std::make_unique<CastCoreWebUI>(webui, host, core_app_service_stub);
}
#endif

GrpcWebUIController::GrpcWebUIController(
    content::WebUI* webui,
    const std::string host,
    cast::v2::CoreApplicationServiceStub* core_app_service_stub)
    : WebUIController(webui),
      web_contents_(webui->GetWebContents()),
      browser_context_(web_contents_->GetBrowserContext()) {
  DCHECK(web_contents_);
  DCHECK(browser_context_);
  webui->SetBindings(content::BINDINGS_POLICY_WEB_UI);
  auto cast_resources = std::make_unique<GrpcResourceDataSource>(
      host, true /* for_webui */, core_app_service_stub);
  if (host == kCastWebUIHomeHost) {
    cast_resources->OverrideContentSecurityPolicyChildSrc(
        kContentSecurityPolicyOverride);
    cast_resources->DisableDenyXFrameOptions();
  }

  content::URLDataSource::Add(browser_context_, std::move(cast_resources));
  auto message_handler = std::make_unique<CastWebUIMessageHandler>();
  message_handler_ = message_handler.get();
  webui->AddMessageHandler(std::move(message_handler));
  RegisterMessageCallbacks();
}

GrpcWebUIController::~GrpcWebUIController() = default;

content::WebContents* GrpcWebUIController::web_contents() const {
  return web_contents_;
}

content::BrowserContext* GrpcWebUIController::browser_context() const {
  return browser_context_;
}

void GrpcWebUIController::StartPingNotify(const base::ListValue* args) {
  CallJavascriptFunction(kJSPingNotifyCallback, {});
}

void GrpcWebUIController::StopPingNotify(const base::ListValue* args) {
  CallJavascriptFunction(kJSEurekaInfoChangedCallback, {});
}

void GrpcWebUIController::SetOobeFinished(const base::ListValue* args) {}

void GrpcWebUIController::RecordAction(const base::ListValue* args) {}

void GrpcWebUIController::LaunchTutorial(const base::ListValue* args) {}

void GrpcWebUIController::GetQRCode(const base::ListValue* args) {}

void GrpcWebUIController::RegisterMessageCallbacks() {
  web_ui()->RegisterDeprecatedMessageCallback(
      kFuncStartPingNotify,
      base::BindRepeating(&GrpcWebUIController::StartPingNotify,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      kFuncStopPingNotify,
      base::BindRepeating(&GrpcWebUIController::StopPingNotify,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      kFuncSetOobeFinished,
      base::BindRepeating(&GrpcWebUIController::SetOobeFinished,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      kFuncRecordAction, base::BindRepeating(&GrpcWebUIController::RecordAction,
                                             base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      kFuncLaunchTutorial,
      base::BindRepeating(&GrpcWebUIController::LaunchTutorial,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      kFuncGetQRCode, base::BindRepeating(&GrpcWebUIController::GetQRCode,
                                          base::Unretained(this)));
}

void GrpcWebUIController::CallJavascriptFunction(
    const std::string& function,
    std::vector<base::Value> args) {
  message_handler_->CallJavascriptFunction(function, std::move(args));
}

}  // namespace chromecast
