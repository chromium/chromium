// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/url_loader_throttle_provider_impl.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/google_url_loader_throttle.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/chrome_render_frame_observer.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/fingerprinting_protection_filter/renderer/renderer_agent.h"
#include "components/fingerprinting_protection_filter/renderer/renderer_url_loader_throttle.h"
#include "components/fingerprinting_protection_filter/renderer/unverified_ruleset_dealer.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_helper.h"
#include "components/safe_browsing/content/renderer/renderer_url_loader_throttle.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/common/content_features.h"
#include "content/public/common/web_identity.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/renderer/extension_localization_throttle.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/modules/credentialmanagement/throttle_helper.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/switches.h"
#include "extensions/renderer/extension_throttle_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/renderer/ash_merge_session_loader_throttle.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
std::unique_ptr<extensions::ExtensionThrottleManager>
CreateExtensionThrottleManager() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          extensions::switches::kDisableExtensionsHttpThrottling)) {
    return nullptr;
  }
  return std::make_unique<extensions::ExtensionThrottleManager>();
}

void SetExtensionThrottleManagerTestPolicy(
    extensions::ExtensionThrottleManager* extension_throttle_manager) {
  std::unique_ptr<net::BackoffEntry::Policy> policy(
      new net::BackoffEntry::Policy{
          // Number of initial errors (in sequence) to ignore before
          // applying exponential back-off rules.
          1,

          // Initial delay for exponential back-off in ms.
          10 * 60 * 1000,

          // Factor by which the waiting time will be multiplied.
          10,

          // Fuzzing percentage. ex: 10% will spread requests randomly
          // between 90%-100% of the calculated time.
          0.1,

          // Maximum amount of time we are willing to delay our request in ms.
          15 * 60 * 1000,

          // Time to keep an entry from being discarded even when it
          // has no significant state, -1 to never discard.
          -1,

          // Don't use initial delay unless the last request was an error.
          false,
      });
  extension_throttle_manager->SetBackoffPolicyForTests(std::move(policy));
}
#endif

}  // namespace

// static
std::unique_ptr<blink::URLLoaderThrottleProvider>
URLLoaderThrottleProviderImpl::Create(
    blink::URLLoaderThrottleProviderType type,
    ChromeContentRendererClient* chrome_content_renderer_client,
    blink::ThreadSafeBrowserInterfaceBrokerProxy* broker) {
  mojo::PendingRemote<safe_browsing::mojom::SafeBrowsing> pending_safe_browsing;
  broker->GetInterface(pending_safe_browsing.InitWithNewPipeAndPassReceiver());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  mojo::PendingRemote<safe_browsing::mojom::ExtensionWebRequestReporter>
      pending_extension_web_request_reporter;
  broker->GetInterface(
      pending_extension_web_request_reporter.InitWithNewPipeAndPassReceiver());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return std::make_unique<URLLoaderThrottleProviderImpl>(
      type, chrome_content_renderer_client, std::move(pending_safe_browsing),
#if BUILDFLAG(ENABLE_EXTENSIONS)
      std::move(pending_extension_web_request_reporter),
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
      /*main_thread_task_runner=*/
      content::RenderThread::IsMainThread()
          ? base::SequencedTaskRunner::GetCurrentDefault()
          : nullptr,
      base::PassKey<URLLoaderThrottleProviderImpl>());
}

URLLoaderThrottleProviderImpl::URLLoaderThrottleProviderImpl(
    blink::URLLoaderThrottleProviderType type,
    ChromeContentRendererClient* chrome_content_renderer_client,
    mojo::PendingRemote<safe_browsing::mojom::SafeBrowsing>
        pending_safe_browsing,
#if BUILDFLAG(ENABLE_EXTENSIONS)
    mojo::PendingRemote<safe_browsing::mojom::ExtensionWebRequestReporter>
        pending_extension_web_request_reporter,
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
    base::PassKey<URLLoaderThrottleProviderImpl>)
    : type_(type),
      chrome_content_renderer_client_(chrome_content_renderer_client),
      pending_safe_browsing_(std::move(pending_safe_browsing)),
#if BUILDFLAG(ENABLE_EXTENSIONS)
      pending_extension_web_request_reporter_(
          std::move(pending_extension_web_request_reporter)),
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
      main_thread_task_runner_(std::move(main_thread_task_runner)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

URLLoaderThrottleProviderImpl::~URLLoaderThrottleProviderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
URLLoaderThrottleProviderImpl::Clone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<URLLoaderThrottleProviderImpl>(
      type_, chrome_content_renderer_client_, CloneSafeBrowsingPendingRemote(),
#if BUILDFLAG(ENABLE_EXTENSIONS)
      CloneExtensionWebRequestReporterPendingRemote(),
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
      main_thread_task_runner_, base::PassKey<URLLoaderThrottleProviderImpl>());
}

blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>>
URLLoaderThrottleProviderImpl::CreateThrottles(
    base::optional_ref<const blink::LocalFrameToken> local_frame_token,
    const network::ResourceRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;

  // Some throttles have already been added in the browser for frame resources.
  // Don't add them for frame requests.
  bool is_frame_resource =
      blink::IsRequestDestinationFrame(request.destination);

  DCHECK(!is_frame_resource ||
         type_ == blink::URLLoaderThrottleProviderType::kFrame);

  if (!is_frame_resource) {
    if (pending_safe_browsing_) {
      safe_browsing_.Bind(std::move(pending_safe_browsing_));
    }
#if BUILDFLAG(ENABLE_EXTENSIONS)
    if (pending_extension_web_request_reporter_) {
      extension_web_request_reporter_.Bind(
          std::move(pending_extension_web_request_reporter_));
    }

    auto throttle = std::make_unique<safe_browsing::RendererURLLoaderThrottle>(
        safe_browsing_.get(), local_frame_token,
        extension_web_request_reporter_.get());
#else
    auto throttle = std::make_unique<safe_browsing::RendererURLLoaderThrottle>(
        safe_browsing_.get(), local_frame_token);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
    throttles.emplace_back(std::move(throttle));
  }

  if (fingerprinting_protection_filter::features::
          IsFingerprintingProtectionFeatureEnabled()) {
    // Restrict the requests that we check as much as possible. This corresponds
    // to a request where:
    //   * The resource requested is not a frame.
    //   * The resource request is made in the context of a frame.
    //   * The request matches our URL filtering criteria.
    //   * There is a valid frame token we can use to retrieve information
    //     about the current `Document`.
    bool should_check_request =
        !is_frame_resource &&
        type_ == blink::URLLoaderThrottleProviderType::kFrame &&
        !fingerprinting_protection_filter::RendererURLLoaderThrottle::
            WillIgnoreRequest(request.url, request.destination) &&
        local_frame_token.has_value();
    if (should_check_request) {
      throttles.emplace_back(
          std::make_unique<
              fingerprinting_protection_filter::RendererURLLoaderThrottle>(
              main_thread_task_runner_, local_frame_token));
    }
  }

  if (type_ == blink::URLLoaderThrottleProviderType::kFrame &&
      !is_frame_resource && local_frame_token.has_value()) {
    auto throttle = prerender::NoStatePrefetchHelper::MaybeCreateThrottle(
        local_frame_token.value());
    if (throttle)
      throttles.emplace_back(std::move(throttle));
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!extension_throttle_manager_)
    extension_throttle_manager_ = CreateExtensionThrottleManager();

  if (extension_throttle_manager_) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            extensions::switches::kSetExtensionThrottleTestParams)) {
      SetExtensionThrottleManagerTestPolicy(extension_throttle_manager_.get());
    }

    std::unique_ptr<blink::URLLoaderThrottle> throttle =
        extension_throttle_manager_->MaybeCreateURLLoaderThrottle(request);
    if (throttle)
      throttles.emplace_back(std::move(throttle));
  }
  std::unique_ptr<blink::URLLoaderThrottle> localization_throttle =
      extensions::ExtensionLocalizationThrottle::MaybeCreate(local_frame_token,
                                                             request.url);
  if (localization_throttle) {
    throttles.emplace_back(std::move(localization_throttle));
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  std::string client_data_header;
  if (!is_frame_resource && local_frame_token.has_value()) {
    client_data_header = ChromeRenderFrameObserver::GetCCTClientHeader(
        local_frame_token.value());
  }
#endif

  throttles.emplace_back(std::make_unique<GoogleURLLoaderThrottle>(
#if BUILDFLAG(IS_ANDROID)
      client_data_header,
#endif
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      chrome_content_renderer_client_->GetChromeObserver()
          ->CreateBoundSessionRequestThrottledHandler(),
#endif
      chrome_content_renderer_client_->GetChromeObserver()
          ->GetDynamicParams()));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  throttles.emplace_back(std::make_unique<AshMergeSessionLoaderThrottle>(
      chrome_content_renderer_client_->GetChromeObserver()
          ->chromeos_listener()));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (local_frame_token.has_value()) {
    auto throttle =
        content::MaybeCreateIdentityUrlLoaderThrottle(base::BindRepeating(
            [](const blink::LocalFrameToken& token,
               const scoped_refptr<base::SequencedTaskRunner>
                   main_thread_task_runner,
               const url::Origin& origin,
               blink::mojom::IdpSigninStatus status) {
              if (content::RenderThread::IsMainThread()) {
                blink::SetIdpSigninStatus(token, origin, status);
                return;
              }
              if (main_thread_task_runner) {
                main_thread_task_runner->PostTask(
                    FROM_HERE, base::BindOnce(&blink::SetIdpSigninStatus, token,
                                              origin, status));
              }
            },
            local_frame_token.value(), main_thread_task_runner_));
    if (throttle)
      throttles.push_back(std::move(throttle));
  }

  return throttles;
}

void URLLoaderThrottleProviderImpl::SetOnline(bool is_online) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extension_throttle_manager_)
    extension_throttle_manager_->SetOnline(is_online);
#endif
}

mojo::PendingRemote<safe_browsing::mojom::SafeBrowsing>
URLLoaderThrottleProviderImpl::CloneSafeBrowsingPendingRemote() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::PendingRemote<safe_browsing::mojom::SafeBrowsing>
      new_pending_safe_browsing;
  if (pending_safe_browsing_) {
    safe_browsing_.Bind(std::move(pending_safe_browsing_));
  }
  if (safe_browsing_) {
    safe_browsing_->Clone(
        new_pending_safe_browsing.InitWithNewPipeAndPassReceiver());
  }
  return new_pending_safe_browsing;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
mojo::PendingRemote<safe_browsing::mojom::ExtensionWebRequestReporter>
URLLoaderThrottleProviderImpl::CloneExtensionWebRequestReporterPendingRemote() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::PendingRemote<safe_browsing::mojom::ExtensionWebRequestReporter>
      new_pending_extension_web_request_reporter;
  if (pending_extension_web_request_reporter_) {
    extension_web_request_reporter_.Bind(
        std::move(pending_extension_web_request_reporter_));
  }
  if (extension_web_request_reporter_) {
    extension_web_request_reporter_->Clone(
        new_pending_extension_web_request_reporter
            .InitWithNewPipeAndPassReceiver());
  }
  return new_pending_extension_web_request_reporter;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
