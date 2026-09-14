// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/search_preload_process_data.h"

#include <memory>

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace page_load_metrics {

const void* const SearchPreloadProcessData::kRenderProcessHostUserDataKey =
    &SearchPreloadProcessData::kRenderProcessHostUserDataKey;

// static
SearchPreloadProcessData* SearchPreloadProcessData::Get(
    content::RenderProcessHost* render_process_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!render_process_host) {
    return nullptr;
  }
  return static_cast<SearchPreloadProcessData*>(
      render_process_host->GetUserData(kRenderProcessHostUserDataKey));
}

// static
SearchPreloadProcessData* SearchPreloadProcessData::GetOrCreate(
    content::RenderProcessHost* render_process_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!render_process_host) {
    return nullptr;
  }
  auto* data = Get(render_process_host);
  if (!data) {
    auto new_data = std::make_unique<SearchPreloadProcessData>();
    data = new_data.get();
    render_process_host->SetUserData(kRenderProcessHostUserDataKey,
                                     std::move(new_data));
  }
  return data;
}

SearchPreloadProcessData::SearchPreloadProcessData() = default;

SearchPreloadProcessData::~SearchPreloadProcessData() = default;

}  // namespace page_load_metrics
