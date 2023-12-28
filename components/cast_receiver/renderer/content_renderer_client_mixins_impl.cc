// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/renderer/content_renderer_client_mixins_impl.h"

#include "components/cast_receiver/renderer/url_rewrite_rules_provider.h"
#include "components/media_control/renderer/media_playback_options.h"
#include "components/on_load_script_injector/renderer/on_load_script_injector.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace cast_receiver {

// static
std::unique_ptr<ContentRendererClientMixins>
ContentRendererClientMixins::Create(
    IsCorsExemptHeadersCallback is_cors_exempt_header_callback) {
  return std::make_unique<ContentRendererClientMixinsImpl>(
      std::move(is_cors_exempt_header_callback));
}

ContentRendererClientMixinsImpl::ContentRendererClientMixinsImpl(
    IsCorsExemptHeadersCallback is_cors_exempt_header_callback)
    : is_cors_exempt_header_callback_(
          std::move(is_cors_exempt_header_callback)) {
  DCHECK(is_cors_exempt_header_callback_);
}

ContentRendererClientMixinsImpl::~ContentRendererClientMixinsImpl() = default;

void ContentRendererClientMixinsImpl::RenderFrameCreated(
    content::RenderFrame& render_frame) {
  // Add script injection support to the RenderFrame, used for bindings support
  // APIs. The injector's lifetime is bound to the RenderFrame's lifetime.
  new on_load_script_injector::OnLoadScriptInjector(&render_frame);

  // Lifetime is tied to |render_frame| via content::RenderFrameObserver.
  new media_control::MediaPlaybackOptions(&render_frame);

  // Create the new UrlRewriteRulesProvider.
  url_rewrite_rules_providers_.emplace(
      render_frame.GetWebFrame()->GetLocalFrameToken(),
      std::make_unique<UrlRewriteRulesProvider>(
          &render_frame,
          base::BindOnce(&ContentRendererClientMixinsImpl::OnRenderFrameRemoved,
                         base::Unretained(this))));
}

bool ContentRendererClientMixinsImpl::DeferMediaLoad(
    content::RenderFrame& render_frame,
    base::OnceClosure closure) {
  auto* playback_options =
      media_control::MediaPlaybackOptions::Get(&render_frame);
  DCHECK(playback_options);
  return playback_options->RunWhenInForeground(std::move(closure));
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
ContentRendererClientMixinsImpl::CreateURLLoaderThrottleProvider() {
  // It is safe to use |this| here because the lifetime of this object is
  // expected to match that of the ContentRendererClient with which it is
  // associated, so it should outlive any WrappingURLLoaderThrottleProvider
  // instances it creates.
  return std::make_unique<WrappingURLLoaderThrottleProvider>(*this);
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
ContentRendererClientMixinsImpl::ExtendURLLoaderThrottleProvider(
    std::unique_ptr<blink::URLLoaderThrottleProvider> delegated_load_provider) {
  DCHECK(delegated_load_provider);
  // It is safe to use |this| here because the lifetime of this object is
  // expected to match that of the ContentRendererClient with which it is
  // associated, so it should outlive any WrappingURLLoaderThrottleProvider
  // instances it creates.
  return std::make_unique<WrappingURLLoaderThrottleProvider>(
      std::move(delegated_load_provider), *this);
}

void ContentRendererClientMixinsImpl::OnRenderFrameRemoved(
    const blink::LocalFrameToken& frame_token) {
  size_t result = url_rewrite_rules_providers_.erase(frame_token);
  if (result != 1U) {
    LOG(WARNING)
        << "Can't find the URL rewrite rules provider for render frame: "
        << frame_token;
  }
}

UrlRewriteRulesProvider*
ContentRendererClientMixinsImpl::GetUrlRewriteRulesProvider(
    const blink::LocalFrameToken& frame_token) {
  auto rules_it = url_rewrite_rules_providers_.find(frame_token);
  return rules_it == url_rewrite_rules_providers_.end()
             ? nullptr
             : rules_it->second.get();
}

bool ContentRendererClientMixinsImpl::IsCorsExemptHeader(
    std::string_view header) {
  return is_cors_exempt_header_callback_.Run(header);
}

}  // namespace cast_receiver
