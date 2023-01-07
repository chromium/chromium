// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/embedder/performance_manager_registry.h"

#include "components/performance_manager/performance_manager_registry_impl.h"
#include "components/performance_manager/performance_manager_tab_helper.h"

namespace performance_manager {

// static
std::unique_ptr<PerformanceManagerRegistry>
PerformanceManagerRegistry::Create() {
  return std::make_unique<PerformanceManagerRegistryImpl>();
}

// static
PerformanceManagerRegistry* PerformanceManagerRegistry::GetInstance() {
  return PerformanceManagerRegistryImpl::GetInstance();
}

void PerformanceManagerRegistry::MaybeCreatePageNodeForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  // Do not attach if we're already attached.
  if (PerformanceManagerTabHelper::FromWebContents(web_contents))
    return;
  CreatePageNodeForWebContents(web_contents);
}

}  // namespace performance_manager
