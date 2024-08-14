// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/capture_mode/search_results_view.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

AshWebView::InitParams GetInitParams() {
  AshWebView::InitParams params;
  params.suppress_navigation = true;
  return params;
}

// Modifies `new_tab_params` to open in a new tab.
void OpenURLFromTabInternal(NavigateParams& new_tab_params) {
  new_tab_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  new_tab_params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&new_tab_params);
}

}  // namespace

SearchResultsView::SearchResultsView() : AshWebViewImpl(GetInitParams()) {
  DCHECK(features::IsSunfishFeatureEnabled());
}

SearchResultsView::~SearchResultsView() = default;

content::WebContents* SearchResultsView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  // Open the URL specified by `params` in a new tab.
  NavigateParams new_tab_params(static_cast<Browser*>(nullptr), params.url,
                                params.transition);
  new_tab_params.FillNavigateParamsFromOpenURLParams(params);
  OpenURLFromTabInternal(new_tab_params);
  return new_tab_params.navigated_or_inserted_contents;
}

BEGIN_METADATA(SearchResultsView)
END_METADATA

}  // namespace ash
