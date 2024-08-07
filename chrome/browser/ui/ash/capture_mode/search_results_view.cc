// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/capture_mode/search_results_view.h"

#include "ash/constants/ash_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

AshWebView::InitParams GetInitParams() {
  AshWebView::InitParams params;
  params.suppress_navigation = true;
  return params;
}

}  // namespace

SearchResultsView::SearchResultsView() : AshWebViewImpl(GetInitParams()) {
  DCHECK(features::IsSunfishFeatureEnabled());
}

SearchResultsView::~SearchResultsView() = default;

BEGIN_METADATA(SearchResultsView)
END_METADATA

}  // namespace ash
