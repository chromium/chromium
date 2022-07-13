// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/lens/lens_side_panel_coordinator.h"
#include <iostream>

#include "base/callback.h"
#include "base/scoped_observation.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/lens/lens_unified_side_panel_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/vector_icons.h"

LensSidePanelCoordinator::LensSidePanelCoordinator(Browser* browser)
    : BrowserUserData(*browser) {
  GetBrowserView()->side_panel_coordinator()->AddSidePanelViewStateObserver(
      this);
  lens_side_panel_view_ = nullptr;
}

BrowserView* LensSidePanelCoordinator::GetBrowserView() {
  return BrowserView::GetBrowserViewForBrowser(&GetBrowser());
}

LensSidePanelCoordinator::~LensSidePanelCoordinator() {
  if (GetBrowserView() && GetBrowserView()->side_panel_coordinator()) {
    GetBrowserView()
        ->side_panel_coordinator()
        ->RemoveSidePanelViewStateObserver(this);
  }
}

void LensSidePanelCoordinator::OnSidePanelDidClose() {
  lens_side_panel_view_ = nullptr;
  GetBrowserView()
      ->side_panel_coordinator()
      ->GetGlobalSidePanelRegistry()
      ->Deregister(SidePanelEntry::Id::kLens);
}

void LensSidePanelCoordinator::RegisterEntryAndShow(
    const content::OpenURLParams& params) {
  auto* side_panel_coordinator = GetBrowserView()->side_panel_coordinator();
  auto* global_registry = side_panel_coordinator->GetGlobalSidePanelRegistry();

  // check if the view is already registered
  if (global_registry->GetEntryForId(SidePanelEntry::Id::kLens) != nullptr &&
      lens_side_panel_view_ != nullptr) {
    lens_side_panel_view_->OpenUrl(params);
  } else {
    global_registry->Register(std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kLens,
        l10n_util::GetStringUTF16(IDS_SIDE_PANEL_COMBO_BOX_GOOGLE_LENS_LABEL),
        ui::ImageModel::FromVectorIcon(kGoogleLensLogoIcon, ui::kColorIcon),
        base::BindRepeating(&LensSidePanelCoordinator::CreateLensWebView,
                            base::Unretained(this), params)));
  }

  if (side_panel_coordinator->GetCurrentEntryId() !=
      SidePanelEntry::Id::kLens) {
    side_panel_coordinator->Show(SidePanelEntry::Id::kLens);
  }
}

content::WebContents* LensSidePanelCoordinator::GetViewWebContentsForTesting() {
  return lens_side_panel_view_ ? lens_side_panel_view_->GetWebContents()
                               : nullptr;
}

bool LensSidePanelCoordinator::OpenResultsInNewTabForTesting() {
  if (lens_side_panel_view_ == nullptr)
    return false;

  lens_side_panel_view_->LoadResultsInNewTab();
  return true;
}

std::unique_ptr<views::View> LensSidePanelCoordinator::CreateLensWebView(
    const content::OpenURLParams& params) {
  auto side_panel_view_ =
      std::make_unique<lens::LensUnifiedSidePanelView>(GetBrowserView());
  side_panel_view_->OpenUrl(params);
  lens_side_panel_view_ = side_panel_view_->GetWeakPtr();
  return side_panel_view_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LensSidePanelCoordinator);
