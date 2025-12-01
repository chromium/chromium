// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_thread_observer.h"

#include <stddef.h>

#include <limits>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/media/media_resource_provider.h"
#include "chrome/common/net/net_resource_provider.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/process_state.h"
#include "components/visitedlink/renderer/visitedlink_reader.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_usage_reporter_type_converters.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/localized_strings.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/net_errors.h"
#include "net/base/net_module.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_view.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/renderer/ash_merge_session_loader_throttle.h"
#endif

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "chrome/renderer/bound_session_credentials/bound_session_request_throttled_handler_renderer_impl.h"
#include "chrome/renderer/bound_session_credentials/bound_session_request_throttled_in_renderer_manager.h"
#endif

using blink::WebCache;
using blink::WebSecurityPolicy;
using content::RenderThread;

#if !BUILDFLAG(IS_ANDROID)
using blink::WebString;
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {

#if BUILDFLAG(IS_CHROMEOS)
scoped_refptr<base::SequencedTaskRunner> GetCallbackGroupTaskRunner() {
  content::ChildThread* child_thread = content::ChildThread::Get();
  if (child_thread)
    return child_thread->GetIOTaskRunner();

  // This will happen when running via tests.
  return base::SequencedTaskRunner::GetCurrentDefault();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
void MaybeRegisterChromeSearchSchemeDisplayIsolated() {
  // The Instant process can only display the content but not read it. Other
  // processes can't display it or read it. (see http://crbug.com/40309067 for
  // more context on why chrome-search scheme registration is skipped for the
  // instant process).
  if (!process_state::IsInstantProcess()) {
    WebString chrome_search_scheme(
        WebString::FromASCII(chrome::kChromeSearchScheme));
    WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(chrome_search_scheme);
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

#if BUILDFLAG(IS_CHROMEOS)
// static
scoped_refptr<ChromeRenderThreadObserver::ChromeOSListener>
ChromeRenderThreadObserver::ChromeOSListener::Create(
    mojo::PendingReceiver<chrome::mojom::ChromeOSListener>
        chromeos_listener_receiver) {
  scoped_refptr<ChromeOSListener> helper = new ChromeOSListener();
  content::ChildThread::Get()->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ChromeOSListener::BindOnIOThread, helper,
                                std::move(chromeos_listener_receiver)));
  return helper;
}

bool ChromeRenderThreadObserver::ChromeOSListener::IsMergeSessionRunning()
    const {
  base::AutoLock lock(lock_);
  return merge_session_running_;
}

void ChromeRenderThreadObserver::ChromeOSListener::RunWhenMergeSessionFinished(
    DelayedCallbackGroup::Callback callback) {
  base::AutoLock lock(lock_);
  DCHECK(merge_session_running_);
  session_merged_callbacks_->Add(std::move(callback));
}

void ChromeRenderThreadObserver::ChromeOSListener::MergeSessionComplete() {
  {
    base::AutoLock lock(lock_);
    merge_session_running_ = false;
  }
  session_merged_callbacks_->RunAll();
}

ChromeRenderThreadObserver::ChromeOSListener::ChromeOSListener()
    : session_merged_callbacks_(base::MakeRefCounted<DelayedCallbackGroup>(
          AshMergeSessionLoaderThrottle::GetMergeSessionTimeout(),
          GetCallbackGroupTaskRunner())),
      merge_session_running_(true) {}

ChromeRenderThreadObserver::ChromeOSListener::~ChromeOSListener() = default;

void ChromeRenderThreadObserver::ChromeOSListener::BindOnIOThread(
    mojo::PendingReceiver<chrome::mojom::ChromeOSListener>
        chromeos_listener_receiver) {
  receiver_.Bind(std::move(chromeos_listener_receiver));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

ChromeRenderThreadObserver::ChromeRenderThreadObserver()
    : visited_link_reader_(new visitedlink::VisitedLinkReader) {
  // Configure modules that need access to resources.
  net::NetModule::SetResourceProvider(ChromeNetResourceProvider);
  media::SetLocalizedStringProvider(ChromeMediaLocalizedStringProvider);
}

ChromeRenderThreadObserver::~ChromeRenderThreadObserver() = default;

chrome::mojom::DynamicParamsPtr ChromeRenderThreadObserver::GetDynamicParams()
    const {
  {
    base::AutoLock lock(dynamic_params_lock_);
    if (dynamic_params_) {
      return dynamic_params_.Clone();
    }
  }
  return chrome::mojom::DynamicParams::New();
}

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
// Returns null if `bound_session_request_throttled_in_renderer_manager_` is
// null. This can happen on profiles where `RendererUpdater` and
// `BoundSessionCookieRefreshService` keyed services are not created.
std::unique_ptr<BoundSessionRequestThrottledHandler>
ChromeRenderThreadObserver::CreateBoundSessionRequestThrottledHandler() const {
  if (!bound_session_request_throttled_in_renderer_manager_) {
    return nullptr;
  }

  return std::make_unique<BoundSessionRequestThrottledHandlerRendererImpl>(
      bound_session_request_throttled_in_renderer_manager_, io_task_runner_);
}
#endif

void ChromeRenderThreadObserver::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->AddInterface<chrome::mojom::RendererConfiguration>(
      base::BindRepeating(
          &ChromeRenderThreadObserver::OnRendererConfigurationAssociatedRequest,
          base::Unretained(this)));
}

void ChromeRenderThreadObserver::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(
      chrome::mojom::RendererConfiguration::Name_);
}

void ChromeRenderThreadObserver::SetInitialConfiguration(
    bool is_incognito_process,
    mojo::PendingReceiver<chrome::mojom::ChromeOSListener>
        chromeos_listener_receiver,
    mojo::PendingRemote<content_settings::mojom::ContentSettingsManager>
        content_settings_manager,
    mojo::PendingRemote<chrome::mojom::BoundSessionRequestThrottledHandler>
        bound_session_request_throttled_handler) {
  if (content_settings_manager)
    content_settings_manager_.Bind(std::move(content_settings_manager));
  process_state::SetIsIncognitoProcess(is_incognito_process);
#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos_listener_receiver) {
    chromeos_listener_ =
        ChromeOSListener::Create(std::move(chromeos_listener_receiver));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (bound_session_request_throttled_handler) {
    bound_session_request_throttled_in_renderer_manager_ =
        BoundSessionRequestThrottledInRendererManager::Create(
            std::move(bound_session_request_throttled_handler));
    io_task_runner_ = content::ChildThread::Get()->GetIOTaskRunner();
  }
#endif
}

void ChromeRenderThreadObserver::SetConfiguration(
    chrome::mojom::DynamicParamsPtr params) {
  base::AutoLock lock(dynamic_params_lock_);
  dynamic_params_ = std::move(params);
}

#if !BUILDFLAG(IS_ANDROID)
void ChromeRenderThreadObserver::SetConfigurationOnProcessLockUpdate(
    chrome::mojom::StaticParamsPtr params) {
  // Ensure static renderer configuration parameters are set once.
  CHECK(!static_renderer_params_set_);
  static_renderer_params_set_ = true;
  process_state::SetIsInstantProcess(params->is_instant_process);
  MaybeRegisterChromeSearchSchemeDisplayIsolated();
  OnProcessReady();
}

void ChromeRenderThreadObserver::OnProcessReady() {
  process_ready_event_.Signal();
}

bool ChromeRenderThreadObserver::IsProcessReady() {
  return process_ready_event_.IsSignaled();
}

bool ChromeRenderThreadObserver::WaitForProcessReady(base::TimeDelta timeout) {
  return process_ready_event_.TimedWait(timeout);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void ChromeRenderThreadObserver::OnRendererConfigurationAssociatedRequest(
    mojo::PendingAssociatedReceiver<chrome::mojom::RendererConfiguration>
        receiver) {
  renderer_configuration_receivers_.Add(this, std::move(receiver));
}
