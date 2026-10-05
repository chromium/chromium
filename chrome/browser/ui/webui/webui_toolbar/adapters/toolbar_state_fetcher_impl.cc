// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/adapters/toolbar_state_fetcher_impl.h"

namespace toolbar_ui_api {

ToolbarStateFetcherImpl::ToolbarStateFetcherImpl(CallbackType state_fetcher)
    : state_fetcher_(std::move(state_fetcher)) {}

ToolbarStateFetcherImpl::~ToolbarStateFetcherImpl() = default;

mojom::ToolbarStatePtr ToolbarStateFetcherImpl::GetToolbarState() {
  return state_fetcher_.Run();
}

}  // namespace toolbar_ui_api
