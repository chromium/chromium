// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_browser_process.h"

#include <utility>

#include "base/logging.h"
#include "build/build_config.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_content_browser_client.h"
#include "chromecast/browser/cast_network_contexts.h"
#include "chromecast/browser/devtools/remote_debugging_server.h"
#include "chromecast/browser/metrics/cast_browser_metrics.h"
#include "chromecast/browser/tts/tts_controller.h"
#include "chromecast/metrics/cast_metrics_service_client.h"
#include "chromecast/net/connectivity_checker.h"
#include "chromecast/service/cast_service.h"
#include "components/prefs/pref_service.h"

#if defined(USE_AURA)

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
#include "chromecast/browser/accessibility/accessibility_manager.h"
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)

#include "chromecast/browser/cast_display_configurator.h"
#include "chromecast/graphics/cast_screen.h"
#endif  // defined(USE_AURA)

namespace chromecast {
namespace shell {

namespace {
CastBrowserProcess* g_instance = NULL;
}  // namespace

// static
CastBrowserProcess* CastBrowserProcess::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

CastBrowserProcess::CastBrowserProcess()
    : cast_content_browser_client_(nullptr),
      net_log_(nullptr) {
  DCHECK(!g_instance);
  g_instance = this;
}

CastBrowserProcess::~CastBrowserProcess() {
  DCHECK_EQ(g_instance, this);

  // TODO(halliwell): investigate having the state that's owned in
  // CastContentBrowserClient (and its internal derived class) be owned in
  // another class that's destructed by this point.
  if (cast_content_browser_client_) {
    cast_content_browser_client_->cast_network_contexts()
        ->OnPrefServiceShutdown();
  }

  if (pref_service_)
    pref_service_->CommitPendingWrite();
  g_instance = NULL;
}

void CastBrowserProcess::SetBrowserContext(
    std::unique_ptr<CastBrowserContext> browser_context) {
  DCHECK(!browser_context_);
  browser_context_.swap(browser_context);
}

void CastBrowserProcess::SetCastContentBrowserClient(
    CastContentBrowserClient* cast_content_browser_client) {
  DCHECK(!cast_content_browser_client_);
  cast_content_browser_client_ = cast_content_browser_client;
}

void CastBrowserProcess::SetCastService(
    std::unique_ptr<CastService> cast_service) {
  DCHECK(!cast_service_);
  cast_service_.swap(cast_service);
}

#if defined(USE_AURA)
void CastBrowserProcess::SetCastScreen(
    std::unique_ptr<CastScreen> cast_screen) {
  DCHECK(!cast_screen_);
  cast_screen_ = std::move(cast_screen);
}

void CastBrowserProcess::SetDisplayConfigurator(
    std::unique_ptr<CastDisplayConfigurator> display_configurator) {
  DCHECK(!display_configurator_);
  display_configurator_ = std::move(display_configurator);
}

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
void CastBrowserProcess::SetAccessibilityManager(
    std::unique_ptr<AccessibilityManager> accessibility_manager) {
  DCHECK(!accessibility_manager_);
  accessibility_manager_ = std::move(accessibility_manager);
}

void CastBrowserProcess::ClearAccessibilityManager() {
  accessibility_manager_.reset();
}

#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)

#endif  // defined(USE_AURA)

void CastBrowserProcess::SetMetricsServiceClient(
    std::unique_ptr<metrics::CastMetricsServiceClient> metrics_service_client) {
  DCHECK(!cast_browser_metrics_);
  cast_browser_metrics_ = std::make_unique<metrics::CastBrowserMetrics>(
      std::move(metrics_service_client));
}

void CastBrowserProcess::SetPrefService(
    std::unique_ptr<PrefService> pref_service) {
  DCHECK(!pref_service_);
  pref_service_.swap(pref_service);
}

void CastBrowserProcess::SetRemoteDebuggingServer(
    std::unique_ptr<RemoteDebuggingServer> remote_debugging_server) {
  DCHECK(!remote_debugging_server_);
  remote_debugging_server_.swap(remote_debugging_server);
}

void CastBrowserProcess::SetConnectivityChecker(
    scoped_refptr<ConnectivityChecker> connectivity_checker) {
  DCHECK(!connectivity_checker_);
  connectivity_checker_.swap(connectivity_checker);
}

void CastBrowserProcess::SetNetLog(net::NetLog* net_log) {
  DCHECK(!net_log_);
  net_log_ = net_log;
}

void CastBrowserProcess::SetTtsController(
    std::unique_ptr<TtsController> tts_controller) {
  DCHECK(!tts_controller_);
  tts_controller_ = std::move(tts_controller);
}

void CastBrowserProcess::SetWebViewFactory(
    CastWebViewFactory* web_view_factory) {
  DCHECK(!web_view_factory_);
  web_view_factory_ = web_view_factory;
}

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
void CastBrowserProcess::AccessibilityStateChanged(bool enabled) {
  cast_service_->AccessibilityStateChanged(enabled);
}
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
}  // namespace shell
}  // namespace chromecast
