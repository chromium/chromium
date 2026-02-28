// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher_impl.h"

namespace toolbar_ui_api {

NavigationControlsStateFetcherImpl::NavigationControlsStateFetcherImpl(
    CallbackType state_fetcher)
    : state_fetcher_(std::move(state_fetcher)) {}

NavigationControlsStateFetcherImpl::~NavigationControlsStateFetcherImpl() =
    default;

mojom::NavigationControlsStatePtr
NavigationControlsStateFetcherImpl::GetNavigationControlsState() {
  return state_fetcher_.Run();
}

}  // namespace toolbar_ui_api
