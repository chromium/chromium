// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_SEARCH_PRELOAD_PROCESS_DATA_H_
#define COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_SEARCH_PRELOAD_PROCESS_DATA_H_

#include "base/supports_user_data.h"

namespace content {
class RenderProcessHost;
}  // namespace content

namespace page_load_metrics {

// Holds information about DSE search preload status on a RenderProcessHost.
class SearchPreloadProcessData : public base::SupportsUserData::Data {
 public:
  static SearchPreloadProcessData* Get(
      content::RenderProcessHost* render_process_host);

  static SearchPreloadProcessData* GetOrCreate(
      content::RenderProcessHost* render_process_host);

  SearchPreloadProcessData();
  ~SearchPreloadProcessData() override;

  SearchPreloadProcessData(const SearchPreloadProcessData&) = delete;
  SearchPreloadProcessData& operator=(const SearchPreloadProcessData&) = delete;

 private:
  // The key to store and retrieve this data from RenderProcessHost.
  static const void* const kRenderProcessHostUserDataKey;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_SEARCH_PRELOAD_PROCESS_DATA_H_
