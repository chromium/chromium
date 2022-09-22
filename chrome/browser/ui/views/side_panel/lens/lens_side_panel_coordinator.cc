// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/lens/lens_side_panel_coordinator.h"
#include <iostream>

#include "base/callback.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/scoped_observation.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/lens/lens_unified_side_panel_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/vector_icons.h"

LensSidePanelCoordinator::LensSidePanelCoordinator(Browser* browser)
    : BrowserUserData(*browser) {
  GetBrowserView()->side_panel_coordinator()->AddSidePanelViewStateObserver(
      this);
  lens_side_panel_view_ = nullptr;

  auto* profile = GetBrowserView()->GetProfile();
  template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile);

  current_default_search_provider_ =
      template_url_service_->GetDefaultSearchProvider();
  if (template_url_service_ != nullptr)
    template_url_service_->AddObserver(this);
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

  if (template_url_service_ != nullptr)
    template_url_service_->RemoveObserver(this);
}

void LensSidePanelCoordinator::DeregisterLensFromSidePanel() {
  lens_side_panel_view_ = nullptr;
  GetBrowserView()
      ->side_panel_coordinator()
      ->GetGlobalSidePanelRegistry()
      ->Deregister(SidePanelEntry::Key(SidePanelEntry::Id::kLens));
}

void LensSidePanelCoordinator::OnSidePanelDidClose() {
  DeregisterLensFromSidePanel();
  base::RecordAction(
      base::UserMetricsAction("LensUnifiedSidePanel.HideSidePanel"));
}

void LensSidePanelCoordinator::OnTemplateURLServiceChanged() {
  // When the default search engine changes, remove lens from the side panel to
  // avoid a potentially mismatched state between the combo box label and lens
  // side panel content.
  const TemplateURL* default_search_provider =
      template_url_service_->GetDefaultSearchProvider();

  if (default_search_provider == current_default_search_provider_)
    return;

  current_default_search_provider_ = default_search_provider;
  DeregisterLensFromSidePanel();
  base::RecordAction(base::UserMetricsAction(
      "LensUnifiedSidePanel.RemoveLensEntry_SearchEngineChanged"));
}

void LensSidePanelCoordinator::OnEntryShown(SidePanelEntry* entry) {
  base::RecordAction(
      base::UserMetricsAction("LensUnifiedSidePanel.LensEntryShown"));
}

void LensSidePanelCoordinator::OnEntryHidden(SidePanelEntry* entry) {
  base::RecordAction(
      base::UserMetricsAction("LensUnifiedSidePanel.LensEntryHidden"));
}

bool LensSidePanelCoordinator::IsLaunchButtonEnabledForTesting() {
  DCHECK(lens_side_panel_view_);
  return lens_side_panel_view_->IsLaunchButtonEnabledForTesting();
}

bool LensSidePanelCoordinator::IsDefaultSearchProviderGoogle() {
  auto* profile = GetBrowserView()->GetProfile();
  return search::DefaultSearchProviderIsGoogle(profile);
}

std::u16string LensSidePanelCoordinator::GetComboboxLabel() {
  if (IsDefaultSearchProviderGoogle())
    return l10n_util::GetStringUTF16(
        IDS_SIDE_PANEL_COMBO_BOX_GOOGLE_LENS_LABEL);

  return GetDefaultSearchEngineName(template_url_service_);
}

const gfx::VectorIcon& LensSidePanelCoordinator::GetComboboxIcon() {
  if (IsDefaultSearchProviderGoogle())
    return vector_icons::kGoogleLensLogoIcon;

  return vector_icons::kImageSearchIcon;
}

void LensSidePanelCoordinator::RegisterEntryAndShow(
    const content::OpenURLParams& params) {
  base::RecordAction(base::UserMetricsAction("LensUnifiedSidePanel.LensQuery"));
  auto* side_panel_coordinator = GetBrowserView()->side_panel_coordinator();
  auto* global_registry = side_panel_coordinator->GetGlobalSidePanelRegistry();

  // check if the view is already registered
  if (global_registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kLens)) != nullptr &&
      lens_side_panel_view_ != nullptr) {
    // The user issued a follow-up Lens query.
    base::RecordAction(
        base::UserMetricsAction("LensUnifiedSidePanel.LensQuery_Followup"));
    lens_side_panel_view_->OpenUrl(params);
  } else {
    base::RecordAction(
        base::UserMetricsAction("LensUnifiedSidePanel.LensQuery_New"));
    auto entry = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kLens, GetComboboxLabel(),
        ui::ImageModel::FromVectorIcon(GetComboboxIcon(), ui::kColorIcon),
        base::BindRepeating(&LensSidePanelCoordinator::CreateLensWebView,
                            base::Unretained(this), params));
    entry->AddObserver(this);
    global_registry->Register(std::move(entry));
  }

  if (side_panel_coordinator->GetCurrentEntryId() !=
      SidePanelEntry::Id::kLens) {
    if (!side_panel_coordinator->IsSidePanelShowing()) {
      base::RecordAction(base::UserMetricsAction(
          "LensUnifiedSidePanel.LensQuery_SidePanelClosed"));
    } else {
      base::RecordAction(base::UserMetricsAction(
          "LensUnifiedSidePanel.LensQuery_SidePanelOpenNonLens"));
    }

    side_panel_coordinator->Show(SidePanelEntry::Id::kLens);
  } else {
    base::RecordAction(base::UserMetricsAction(
        "LensUnifiedSidePanel.LensQuery_SidePanelOpenLens"));
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
