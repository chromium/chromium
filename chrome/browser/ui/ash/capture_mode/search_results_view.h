
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CAPTURE_MODE_SEARCH_RESULTS_VIEW_H_
#define CHROME_BROWSER_UI_ASH_CAPTURE_MODE_SEARCH_RESULTS_VIEW_H_

#include "chrome/browser/ui/ash/web_view/ash_web_view_impl.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// `SearchResultsView` is an `AshWebViewImpl` that implements custom handling
// for search results.
class SearchResultsView : public AshWebViewImpl {
  METADATA_HEADER(SearchResultsView, AshWebViewImpl)

 public:
  SearchResultsView();
  SearchResultsView(SearchResultsView&) = delete;
  SearchResultsView& operator=(SearchResultsView&) = delete;
  ~SearchResultsView() override;

  // AshWebViewImpl:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_CAPTURE_MODE_SEARCH_RESULTS_VIEW_H_
