// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_DECORATORS_UTILS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_DECORATORS_UTILS_H_

#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_thread.h"

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
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> node,
             void (decorator_data_type::*setter_function)(T), T value) {
            if (node) {
              auto* data = decorator_data_type::GetOrCreate(
                  PageNodeImpl::FromNode(node.get()));
              DCHECK(data);
              (data->*setter_function)(value);
            }
          },
          PerformanceManager::GetPrimaryPageNodeForWebContents(contents),
          setter_function, value));
}

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_DECORATORS_UTILS_H_
