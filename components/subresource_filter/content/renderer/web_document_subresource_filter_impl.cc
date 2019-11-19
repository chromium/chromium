// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/web_document_subresource_filter_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

namespace proto = url_pattern_index::proto;

namespace {

using WebLoadPolicy = blink::WebDocumentSubresourceFilter::LoadPolicy;

proto::ElementType ToElementType(
    blink::mojom::RequestContextType request_context) {
  switch (request_context) {
    case blink::mojom::RequestContextType::AUDIO:
    case blink::mojom::RequestContextType::VIDEO:
    case blink::mojom::RequestContextType::TRACK:
      return proto::ELEMENT_TYPE_MEDIA;
    case blink::mojom::RequestContextType::BEACON:
    case blink::mojom::RequestContextType::PING:
      return proto::ELEMENT_TYPE_PING;
    case blink::mojom::RequestContextType::EMBED:
    case blink::mojom::RequestContextType::OBJECT:
    case blink::mojom::RequestContextType::PLUGIN:
      return proto::ELEMENT_TYPE_OBJECT;
    case blink::mojom::RequestContextType::EVENT_SOURCE:
    case blink::mojom::RequestContextType::FETCH:
    case blink::mojom::RequestContextType::XML_HTTP_REQUEST:
      return proto::ELEMENT_TYPE_XMLHTTPREQUEST;
    case blink::mojom::RequestContextType::FAVICON:
    case blink::mojom::RequestContextType::IMAGE:
    case blink::mojom::RequestContextType::IMAGE_SET:
      return proto::ELEMENT_TYPE_IMAGE;
    case blink::mojom::RequestContextType::FONT:
      return proto::ELEMENT_TYPE_FONT;
    case blink::mojom::RequestContextType::FRAME:
    case blink::mojom::RequestContextType::FORM:
    case blink::mojom::RequestContextType::HYPERLINK:
    case blink::mojom::RequestContextType::IFRAME:
    case blink::mojom::RequestContextType::INTERNAL:
    case blink::mojom::RequestContextType::LOCATION:
      return proto::ELEMENT_TYPE_SUBDOCUMENT;
    case blink::mojom::RequestContextType::SCRIPT:
    case blink::mojom::RequestContextType::SERVICE_WORKER:
    case blink::mojom::RequestContextType::SHARED_WORKER:
      return proto::ELEMENT_TYPE_SCRIPT;
    case blink::mojom::RequestContextType::STYLE:
    case blink::mojom::RequestContextType::XSLT:
      return proto::ELEMENT_TYPE_STYLESHEET;

    case blink::mojom::RequestContextType::PREFETCH:
    case blink::mojom::RequestContextType::SUBRESOURCE:
      return proto::ELEMENT_TYPE_OTHER;

    case blink::mojom::RequestContextType::CSP_REPORT:
    case blink::mojom::RequestContextType::DOWNLOAD:
    case blink::mojom::RequestContextType::IMPORT:
    case blink::mojom::RequestContextType::MANIFEST:
    case blink::mojom::RequestContextType::UNSPECIFIED:
    default:
      return proto::ELEMENT_TYPE_UNSPECIFIED;
  }
}

WebLoadPolicy ToWebLoadPolicy(LoadPolicy load_policy) {
  switch (load_policy) {
    case LoadPolicy::ALLOW:
      return WebLoadPolicy::kAllow;
    case LoadPolicy::DISALLOW:
      return WebLoadPolicy::kDisallow;
    case LoadPolicy::WOULD_DISALLOW:
      return WebLoadPolicy::kWouldDisallow;
    default:
      NOTREACHED();
      return WebLoadPolicy::kAllow;
  }
}

void ProxyToTaskRunner(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                       base::OnceClosure callback) {
  task_runner->PostTask(FROM_HERE, std::move(callback));
}

}  // namespace

WebDocumentSubresourceFilterImpl::~WebDocumentSubresourceFilterImpl() = default;

WebDocumentSubresourceFilterImpl::WebDocumentSubresourceFilterImpl(
    url::Origin document_origin,
    mojom::ActivationState activation_state,
    scoped_refptr<const MemoryMappedRuleset> ruleset,
    base::OnceClosure first_disallowed_load_callback)
    : activation_state_(activation_state),
      filter_(std::move(document_origin), activation_state, std::move(ruleset)),
      first_disallowed_load_callback_(
          std::move(first_disallowed_load_callback)) {}

WebLoadPolicy WebDocumentSubresourceFilterImpl::GetLoadPolicy(
    const blink::WebURL& resourceUrl,
    blink::mojom::RequestContextType request_context) {
  return getLoadPolicyImpl(resourceUrl, ToElementType(request_context));
}

WebLoadPolicy
WebDocumentSubresourceFilterImpl::GetLoadPolicyForWebSocketConnect(
    const blink::WebURL& url) {
  DCHECK(url.ProtocolIs("ws") || url.ProtocolIs("wss"));
  return getLoadPolicyImpl(url, proto::ELEMENT_TYPE_WEBSOCKET);
}

void WebDocumentSubresourceFilterImpl::ReportDisallowedLoad() {
  if (!first_disallowed_load_callback_.is_null())
    std::move(first_disallowed_load_callback_).Run();
}

bool WebDocumentSubresourceFilterImpl::ShouldLogToConsole() {
  return activation_state().enable_logging;
}

WebLoadPolicy WebDocumentSubresourceFilterImpl::getLoadPolicyImpl(
    const blink::WebURL& url,
    proto::ElementType element_type) {
  if (filter_.activation_state().filtering_disabled_for_document ||
      url.ProtocolIs(url::kDataScheme)) {
    ++filter_.statistics().num_loads_total;
    return WebLoadPolicy::kAllow;
  }

  // TODO(pkalinnikov): Would be good to avoid converting to GURL.
  return ToWebLoadPolicy(filter_.GetLoadPolicy(GURL(url), element_type));
}

WebDocumentSubresourceFilterImpl::BuilderImpl::BuilderImpl(
    url::Origin document_origin,
    mojom::ActivationState activation_state,
    base::File ruleset_file,
    base::OnceClosure first_disallowed_load_callback)
    : document_origin_(std::move(document_origin)),
      activation_state_(std::move(activation_state)),
      ruleset_file_(std::move(ruleset_file)),
      first_disallowed_load_callback_(
          std::move(first_disallowed_load_callback)),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}
WebDocumentSubresourceFilterImpl::BuilderImpl::~BuilderImpl() {}

std::unique_ptr<blink::WebDocumentSubresourceFilter>
WebDocumentSubresourceFilterImpl::BuilderImpl::Build() {
  DCHECK(ruleset_file_.IsValid());
  scoped_refptr<MemoryMappedRuleset> ruleset =
      MemoryMappedRuleset::CreateAndInitialize(std::move(ruleset_file_));
  if (!ruleset)
    return nullptr;
  return std::make_unique<WebDocumentSubresourceFilterImpl>(
      document_origin_, activation_state_, std::move(ruleset),
      base::BindOnce(&ProxyToTaskRunner, main_task_runner_,
                     std::move(first_disallowed_load_callback_)));
}

void WebDocumentSubresourceFilterImpl::ReportAdRequestId(int request_id) {
  if (!ad_resource_tracker_)
    return;
  ad_resource_tracker_->NotifyAdResourceObserved(request_id);
}

}  // namespace subresource_filter
