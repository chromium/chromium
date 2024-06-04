// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/page_context.h"

#include <utility>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace resource_attribution {

using PerformanceManagerTabHelper =
    performance_manager::PerformanceManagerTabHelper;

PageContext::PageContext(base::UnguessableToken token,
                         base::WeakPtr<content::WebContents> weak_web_contents,
                         base::WeakPtr<PageNode> weak_node)
    : token_(std::move(token)),
      weak_web_contents_(std::move(weak_web_contents)),
      weak_node_(std::move(weak_node)) {
  CHECK(!token_.is_empty());
}

PageContext::~PageContext() = default;

PageContext::PageContext(const PageContext& other) = default;

PageContext& PageContext::operator=(const PageContext& other) = default;

PageContext::PageContext(PageContext&& other) = default;

PageContext& PageContext::operator=(PageContext&& other) = default;

// static
std::optional<PageContext> PageContext::FromWebContents(
    content::WebContents* contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!contents) {
    return std::nullopt;
  }
  auto* tab_helper = PerformanceManagerTabHelper::FromWebContents(contents);
  if (!tab_helper) {
    return std::nullopt;
  }
  PageNodeImpl* node_impl = tab_helper->primary_page_node();
  CHECK(node_impl);
  return PageContext(node_impl->page_token().value(), contents->GetWeakPtr(),
                     node_impl->GetWeakPtrOnUIThread());
}

content::WebContents* PageContext::GetWebContents() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return weak_web_contents_.get();
}

base::WeakPtr<PageNode> PageContext::GetWeakPageNode() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return weak_node_;
}

// static
PageContext PageContext::FromPageNode(const PageNode* node) {
  CHECK(node);
  DCHECK_ON_GRAPH_SEQUENCE(node->GetGraph());
  auto* node_impl = PageNodeImpl::FromNode(node);

  return PageContext(node_impl->page_token().value(),
                     node_impl->GetWebContents(), node_impl->GetWeakPtr());
}

// static
std::optional<PageContext> PageContext::FromWeakPageNode(
    base::WeakPtr<PageNode> node) {
  if (!node) {
    return std::nullopt;
  }
  return FromPageNode(node.get());
}

PageNode* PageContext::GetPageNode() const {
  if (weak_node_) {
    // `weak_node` will check anyway if dereferenced from the wrong sequence,
    // but let's be explicit.
    DCHECK_ON_GRAPH_SEQUENCE(weak_node_->GetGraph());
    return weak_node_.get();
  }
  return nullptr;
}

std::string PageContext::ToString() const {
  return base::StrCat({"PageContext:", token_.ToString()});
}

}  // namespace resource_attribution
