// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_tab_selection_listener.h"

OmniboxPopupTabSelectionListener::OmniboxPopupTabSelectionListener(
    base::WeakPtr<WebUIContentsWrapper::Host> host,
    TabStripModel* tab_strip_model)
    : host_(std::move(host)) {
  CHECK(tab_strip_model);
  tab_strip_model->AddObserver(this);
}

OmniboxPopupTabSelectionListener::~OmniboxPopupTabSelectionListener() = default;

void OmniboxPopupTabSelectionListener::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (tab_strip_model->empty() || !selection.active_tab_changed()) {
    return;
  }

  if (host_) {
    host_->CloseUI();
  }
}
