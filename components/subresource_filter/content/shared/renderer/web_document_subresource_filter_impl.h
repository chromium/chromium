// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_RENDERER_WEB_DOCUMENT_SUBRESOURCE_FILTER_IMPL_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_RENDERER_WEB_DOCUMENT_SUBRESOURCE_FILTER_IMPL_H_

#include <memory>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "url/origin.h"

namespace subresource_filter {

class MemoryMappedRuleset;
// Performs filtering of subresource loads in the scope of a given document.
class WebDocumentSubresourceFilterImpl final
    : public blink::WebDocumentSubresourceFilter {
 public:
  // This builder class is used for creating the subresource filter for workers
  // and worklets. For workers and threaded worklets, this is created on the
  // main thread and passed to a worker thread. For main thread worklets, this
  // is created and used on the main thread.
  class BuilderImpl : public blink::WebDocumentSubresourceFilter::Builder {
   public:
    BuilderImpl(url::Origin document_origin,
                mojom::ActivationState activation_state,
                base::File ruleset_file,
                base::OnceClosure first_disallowed_load_callback);

    BuilderImpl(const BuilderImpl&) = delete;
    BuilderImpl& operator=(const BuilderImpl&) = delete;

    ~BuilderImpl() override;

    std::unique_ptr<blink::WebDocumentSubresourceFilter> Build() override;

   private:
    url::Origin document_origin_;
    mojom::ActivationState activation_state_;
    base::File ruleset_file_;
    base::OnceClosure first_disallowed_load_callback_;
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  };

  // See DocumentSubresourceFilter description.
  //
  // Invokes |first_disallowed_load_callback|, if it is non-null, on the first
  // reportDisallowedLoad() call.
  WebDocumentSubresourceFilterImpl(
      url::Origin document_origin,
      mojom::ActivationState activation_state,
      scoped_refptr<const MemoryMappedRuleset> ruleset,
      base::OnceClosure first_disallowed_load_callback);

  WebDocumentSubresourceFilterImpl(const WebDocumentSubresourceFilterImpl&) =
      delete;
  WebDocumentSubresourceFilterImpl& operator=(
      const WebDocumentSubresourceFilterImpl&) = delete;

  ~WebDocumentSubresourceFilterImpl() override;

  const DocumentSubresourceFilter& filter() const { return filter_; }

  // blink::WebDocumentSubresourceFilter:
  LoadPolicy GetLoadPolicy(const blink::WebURL& resourceUrl,
                           blink::mojom::RequestContextType) override;
  LoadPolicy GetLoadPolicyForWebSocketConnect(
      const blink::WebURL& url) override;
  LoadPolicy GetLoadPolicyForWebTransportConnect(
      const blink::WebURL& url) override;
  void ReportDisallowedLoad() override;
  bool ShouldLogToConsole() override;

  const mojom::ActivationState& activation_state() const {
    return filter_.activation_state();
  }

  base::WeakPtr<WebDocumentSubresourceFilterImpl> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  LoadPolicy getLoadPolicyImpl(
      const blink::WebURL& url,
      url_pattern_index::proto::ElementType element_type);

  mojom::ActivationState activation_state_;
  DocumentSubresourceFilter filter_;
  base::OnceClosure first_disallowed_load_callback_;
  base::WeakPtrFactory<WebDocumentSubresourceFilterImpl> weak_ptr_factory_{
      this};
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_RENDERER_WEB_DOCUMENT_SUBRESOURCE_FILTER_IMPL_H_
