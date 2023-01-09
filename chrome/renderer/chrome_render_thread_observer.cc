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
#include "base/files/file_util.h"
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
#include "build/chromeos_buildflags.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/media/media_resource_provider.h"
#include "chrome/common/net/net_resource_provider.h"
#include "chrome/common/url_constants.h"
#include "components/visitedlink/renderer/visitedlink_reader.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_usage_reporter_type_converters.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "ipc/ipc_sync_channel.h"
#include "media/base/localized_strings.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/net_errors.h"
#include "net/base/net_module.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_request_peer.h"
#include "third_party/blink/public/platform/web_resource_request_sender_delegate.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_view.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/renderer/localization_peer.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/renderer/chromeos_merge_session_loader_throttle.h"
#endif

using blink::WebCache;
using blink::WebSecurityPolicy;
using content::RenderThread;

namespace {

class RendererResourceDelegate
    : public blink::WebResourceRequestSenderDelegate {
 public:
  RendererResourceDelegate() = default;

  RendererResourceDelegate(const RendererResourceDelegate&) = delete;
  RendererResourceDelegate& operator=(const RendererResourceDelegate&) = delete;

  void OnRequestComplete() override {}
  scoped_refptr<blink::WebRequestPeer> OnReceivedResponse(
      scoped_refptr<blink::WebRequestPeer> current_peer,
      const blink::WebString& mime_type,
      const blink::WebURL& url) override {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    return ExtensionLocalizationPeer::CreateExtensionLocalizationPeer(
        std::move(current_peer), RenderThread::Get(), mime_type.Utf8(), url);
#else
    return current_peer;
#endif
  }

 private:
  base::WeakPtrFactory<RendererResourceDelegate> weak_factory_{this};
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
scoped_refptr<base::SequencedTaskRunner> GetCallbackGroupTaskRunner() {
  content::ChildThread* child_thread = content::ChildThread::Get();
  if (child_thread)
    return child_thread->GetIOTaskRunner();

  // This will happen when running via tests.
  return base::SequencedTaskRunner::GetCurrentDefault();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

bool ChromeRenderThreadObserver::is_incognito_process_ = false;

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
          MergeSessionLoaderThrottle::GetMergeSessionTimeout(),
          GetCallbackGroupTaskRunner())),
      merge_session_running_(true) {}

ChromeRenderThreadObserver::ChromeOSListener::~ChromeOSListener() {}

void ChromeRenderThreadObserver::ChromeOSListener::BindOnIOThread(
    mojo::PendingReceiver<chrome::mojom::ChromeOSListener>
        chromeos_listener_receiver) {
  receiver_.Bind(std::move(chromeos_listener_receiver));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

chrome::mojom::DynamicParams* GetDynamicConfigParams() {
  static base::NoDestructor<chrome::mojom::DynamicParams> dynamic_params;
  return dynamic_params.get();
}

ChromeRenderThreadObserver::ChromeRenderThreadObserver()
    : resource_request_sender_delegate_(
          std::make_unique<RendererResourceDelegate>()),
      visited_link_reader_(new visitedlink::VisitedLinkReader) {
  RenderThread* thread = RenderThread::Get();
  thread->SetResourceRequestSenderDelegate(
      resource_request_sender_delegate_.get());

  // Configure modules that need access to resources.
  net::NetModule::SetResourceProvider(ChromeNetResourceProvider);
  media::SetLocalizedStringProvider(ChromeMediaLocalizedStringProvider);
}

ChromeRenderThreadObserver::~ChromeRenderThreadObserver() {}

// static
const chrome::mojom::DynamicParams&
ChromeRenderThreadObserver::GetDynamicParams() {
  return *GetDynamicConfigParams();
}

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
        content_settings_manager) {
  if (content_settings_manager)
    content_settings_manager_.Bind(std::move(content_settings_manager));
  is_incognito_process_ = is_incognito_process;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos_listener_receiver) {
    chromeos_listener_ =
        ChromeOSListener::Create(std::move(chromeos_listener_receiver));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ChromeRenderThreadObserver::SetConfiguration(
    chrome::mojom::DynamicParamsPtr params) {
  *GetDynamicConfigParams() = std::move(*params);
}

void ChromeRenderThreadObserver::OnRendererConfigurationAssociatedRequest(
    mojo::PendingAssociatedReceiver<chrome::mojom::RendererConfiguration>
        receiver) {
  renderer_configuration_receivers_.Add(this, std::move(receiver));
}
