// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/shared/renderer/web_document_subresource_filter_impl.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_runner.h"
#include "components/subresource_filter/content/shared/renderer/filter_utils.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

namespace proto = url_pattern_index::proto;

namespace {

using WebLoadPolicy = blink::WebDocumentSubresourceFilter::LoadPolicy;

WebLoadPolicy ToWebLoadPolicy(LoadPolicy load_policy) {
  switch (load_policy) {
    case LoadPolicy::EXPLICITLY_ALLOW:
      [[fallthrough]];
    case LoadPolicy::ALLOW:
      return WebLoadPolicy::kAllow;
    case LoadPolicy::DISALLOW:
      return WebLoadPolicy::kDisallow;
    case LoadPolicy::WOULD_DISALLOW:
      return WebLoadPolicy::kWouldDisallow;
    default:
      NOTREACHED_IN_MIGRATION();
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
  CHECK(url.ProtocolIs("ws") || url.ProtocolIs("wss"),
        base::NotFatalUntil::M129);
  return getLoadPolicyImpl(url, proto::ELEMENT_TYPE_WEBSOCKET);
}

WebLoadPolicy
WebDocumentSubresourceFilterImpl::GetLoadPolicyForWebTransportConnect(
    const blink::WebURL& url) {
  return getLoadPolicyImpl(url, proto::ELEMENT_TYPE_WEBTRANSPORT);
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
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}
WebDocumentSubresourceFilterImpl::BuilderImpl::~BuilderImpl() {}

std::unique_ptr<blink::WebDocumentSubresourceFilter>
WebDocumentSubresourceFilterImpl::BuilderImpl::Build() {
  CHECK(ruleset_file_.IsValid(), base::NotFatalUntil::M129);
  scoped_refptr<MemoryMappedRuleset> ruleset =
      MemoryMappedRuleset::CreateAndInitialize(std::move(ruleset_file_));
  if (!ruleset)
    return nullptr;
  return std::make_unique<WebDocumentSubresourceFilterImpl>(
      document_origin_, activation_state_, std::move(ruleset),
      base::BindOnce(&ProxyToTaskRunner, main_task_runner_,
                     std::move(first_disallowed_load_callback_)));
}

}  // namespace subresource_filter
