// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_tab_selection_listener.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"

OmniboxPopupTabSelectionListener::OmniboxPopupTabSelectionListener(
    base::WeakPtr<OmniboxPopupWebUIBaseContent> host,
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

  // For the V2 full popup, tab change events are handled explicitly by the View
  // (OmniboxPopupViewFullWebUI::OnTabChanged) via LocationBarView::Update.
  // We return early here to prevent this listener from automatically closing
  // the UI.
  if (selection.new_contents &&
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopupV2)) {
    return;
  }

  if (host_) {
    host_->CloseUI();
  }
}
