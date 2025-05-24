// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/freezer.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_traits.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {

void Freezer::MaybeFreezePageNode(const PageNode* page_node) {
  DCHECK(page_node);

  base::WeakPtr<content::WebContents> contents = page_node->GetWebContents();
  CHECK(contents);

  // A visible page should not be frozen.
  if (contents->GetVisibility() == content::Visibility::VISIBLE) {
    return;
  }

  contents->SetPageFrozen(true);
}

void Freezer::UnfreezePageNode(const PageNode* page_node) {
  DCHECK(page_node);

  base::WeakPtr<content::WebContents> contents = page_node->GetWebContents();
  CHECK(contents);

  // A visible page is automatically unfrozen.
  if (contents->GetVisibility() == content::Visibility::VISIBLE) {
    return;
  }

  contents->SetPageFrozen(false);
}

}  // namespace performance_manager
