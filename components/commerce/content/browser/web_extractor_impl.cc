// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/content/browser/web_extractor_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "components/commerce/content/browser/web_contents_wrapper.h"
#include "components/grit/components_resources.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "ui/base/resource/resource_bundle.h"

namespace commerce {

WebExtractorImpl::WebExtractorImpl() = default;
WebExtractorImpl::~WebExtractorImpl() = default;

void WebExtractorImpl::ExtractMetaInfo(
    WebWrapper* web_wrapper,
    base::OnceCallback<void(const base::Value)> callback) {
  commerce::WebContentsWrapper* wrapper =
      static_cast<commerce::WebContentsWrapper*>(web_wrapper);
  content::RenderFrameHost* rfh = wrapper->GetPrimaryMainFrame();
  if (!rfh) {
    return;
  }
  mojo::Remote<commerce_web_extractor::mojom::CommerceWebExtractor>
      remote_extractor;
  rfh->GetRemoteInterfaces()->GetInterface(
      remote_extractor.BindNewPipeAndPassReceiver());
  commerce_web_extractor::mojom::CommerceWebExtractor* raw_commerce_extractor =
      remote_extractor.get();
  if (!raw_commerce_extractor) {
    return;
  }
  // Pass along the remote_extractor to keep the pipe alive until the callback
  // returns.
  raw_commerce_extractor->ExtractMetaInfo(base::BindOnce(
      &WebExtractorImpl::OnExtractionMetaInfo, weak_ptr_factory_.GetWeakPtr(),
      std::move(remote_extractor), std::move(callback)));
}

void WebExtractorImpl::OnExtractionMetaInfo(
    mojo::Remote<commerce_web_extractor::mojom::CommerceWebExtractor> extractor,
    base::OnceCallback<void(base::Value)> callback,
    base::Value result) {
  std::move(callback).Run(std::move(result));
}

}  // namespace commerce
