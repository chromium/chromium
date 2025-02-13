// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_DECORATORS_UTILS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_DECORATORS_UTILS_H_

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {

// Helper function meant to be used by decorators tracking properties associated
// with WebContents. This will do the WebContents to PageNode translation and
// post a task to the PM sequence to set a property on the appropriate
// decorator.
// This function can only be called from the UI thread.
template <typename T, class decorator_data_type>
void SetPropertyForWebContentsPageNode(
    content::WebContents* contents,
    void (decorator_data_type::*setter_function)(T),
    T value) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(contents);
  if (!node) {
    // The PageNode for `contents` may already be deleted if a property is
    // being reset from the WebContents destructor, for example by a
    // WebContentsDestroyed observer.
    return;
  }
  auto* data =
      decorator_data_type::GetOrCreate(PageNodeImpl::FromNode(node.get()));
  DCHECK(data);
  (data->*setter_function)(value);
}

// Helper function to return a property from a decorator associated with
// WebContents. This will do the WebContents to PageNode translation and read
// the property from the appropriate decorator.
// This function can only be called from the UI thread.
template <typename T, class decorator_data_type>
T GetPropertyForWebContentsPageNode(content::WebContents* contents,
                                    T (decorator_data_type::*getter_function)()
                                        const) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(contents);
  const auto* data =
      decorator_data_type::GetOrCreate(PageNodeImpl::FromNode(node.get()));
  DCHECK(data);
  return (data->*getter_function)();
}

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_DECORATORS_UTILS_H_
