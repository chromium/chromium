// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_URL_LOADER_THROTTLE_PROVIDER_IMPL_H_
#define CHROME_RENDERER_URL_LOADER_THROTTLE_PROVIDER_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/renderer/extension_throttle_manager.h"
#endif

namespace base {
class SequencedTaskRunner;
}  // namespace base

class ChromeContentRendererClient;

// Instances must be constructed on the render main thread, and then used and
// destructed on a single sequence, which can be different from the render main
// thread.
class URLLoaderThrottleProviderImpl : public blink::URLLoaderThrottleProvider {
 public:
  static std::unique_ptr<blink::URLLoaderThrottleProvider> Create(
      blink::URLLoaderThrottleProviderType type,
      ChromeContentRendererClient* chrome_content_renderer_client,
      blink::ThreadSafeBrowserInterfaceBrokerProxy* broker);

  URLLoaderThrottleProviderImpl(
      blink::URLLoaderThrottleProviderType type,
      ChromeContentRendererClient* chrome_content_renderer_client,
      mojo::PendingRemote<safe_browsing::mojom::SafeBrowsing>
          pending_safe_browsing,
#if BUILDFLAG(ENABLE_EXTENSIONS)
      mojo::PendingRemote<safe_browsing::mojom::ExtensionWebRequestReporter>
          pending_extension_web_request_reporter,
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      base::PassKey<URLLoaderThrottleProviderImpl>);

  URLLoaderThrottleProviderImpl& operator=(
      const URLLoaderThrottleProviderImpl&) = delete;

  ~URLLoaderThrottleProviderImpl() override;

  // blink::URLLoaderThrottleProvider implementation.
  std::unique_ptr<blink::URLLoaderThrottleProvider> Clone() override;
  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      base::optional_ref<const blink::LocalFrameToken> local_frame_token,
      const network::ResourceRequest& request) override;
  void SetOnline(bool is_online) override;

 private:
  mojo::PendingRemote<safe_browsing::mojom::SafeBrowsing>
  CloneSafeBrowsingPendingRemote();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  mojo::PendingRemote<safe_browsing::mojom::ExtensionWebRequestReporter>
  CloneExtensionWebRequestReporterPendingRemote();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  blink::URLLoaderThrottleProviderType type_;
  const raw_ptr<ChromeContentRendererClient> chrome_content_renderer_client_;

  mojo::PendingRemote<safe_browsing::mojom::SafeBrowsing>
      pending_safe_browsing_;
  mojo::Remote<safe_browsing::mojom::SafeBrowsing> safe_browsing_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  mojo::PendingRemote<safe_browsing::mojom::ExtensionWebRequestReporter>
      pending_extension_web_request_reporter_;
  mojo::Remote<safe_browsing::mojom::ExtensionWebRequestReporter>
      extension_web_request_reporter_;

  std::unique_ptr<extensions::ExtensionThrottleManager>
      extension_throttle_manager_;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // Set only when `this` was created on the main thread, or cloned from a
  // provider which was created on the main thread.
  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_RENDERER_URL_LOADER_THROTTLE_PROVIDER_IMPL_H_
