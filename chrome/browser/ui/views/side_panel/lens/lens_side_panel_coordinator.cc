// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/lens/lens_side_panel_coordinator.h"
#include <iostream>

#include "base/functional/callback.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/scoped_observation.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/side_panel/companion/companion_tab_helper.h"
#include "chrome/browser/ui/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/lens/lens_unified_side_panel_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/grit/generated_resources.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/image/image.h"
#include "ui/views/vector_icons.h"

LensSidePanelCoordinator::LensSidePanelCoordinator(Browser* browser)
    : BrowserUserData(*browser) {
  GetSidePanelCoordinator()->AddSidePanelViewStateObserver(this);
  lens_side_panel_view_ = nullptr;
  auto* profile = browser->profile();
  favicon_cache_ = std::make_unique<FaviconCache>(
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS));
  template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile);
  current_default_search_provider_ =
      template_url_service_->GetDefaultSearchProvider();
  template_url_service_->AddObserver(this);
}

BrowserView* LensSidePanelCoordinator::GetBrowserView() {
  return BrowserView::GetBrowserViewForBrowser(&GetBrowser());
}

SidePanelCoordinator* LensSidePanelCoordinator::GetSidePanelCoordinator() {
  return SidePanelUtil::GetSidePanelCoordinatorForBrowser(&GetBrowser());
}

LensSidePanelCoordinator::~LensSidePanelCoordinator() {
  if (SidePanelCoordinator* side_panel_coordinator =
          GetSidePanelCoordinator()) {
    side_panel_coordinator->RemoveSidePanelViewStateObserver(this);
  }

  if (template_url_service_ != nullptr)
    template_url_service_->RemoveObserver(this);
}

void LensSidePanelCoordinator::DeregisterLensFromSidePanel() {
  lens_side_panel_view_ = nullptr;
  auto* active_web_contents = GetBrowserView()->GetActiveWebContents();
  const bool should_use_contextual_panel =
      companion::ShouldUseContextualLensPanelForImageSearch(&GetBrowser());

  // Delete contextual lens view if applicable.
  if (should_use_contextual_panel && active_web_contents) {
    auto* companion_helper =
        companion::CompanionTabHelper::FromWebContents(active_web_contents);
    if (companion_helper) {
      companion_helper->RemoveContextualLensView();
    }
  }

  // Remove entry from side panel entry if it exists.
  auto* registry =
      should_use_contextual_panel
          ? SidePanelRegistry::Get(active_web_contents)
          : SidePanelCoordinator::GetGlobalSidePanelRegistry(&GetBrowser());
  if (registry) {
    registry->Deregister(SidePanelEntry::Key(SidePanelEntry::Id::kLens));
  }
}

void LensSidePanelCoordinator::OnSidePanelDidClose() {
  DeregisterLensFromSidePanel();
  base::RecordAction(
      base::UserMetricsAction("LensUnifiedSidePanel.HideSidePanel"));
}

void LensSidePanelCoordinator::OnFaviconFetched(const gfx::Image& favicon) {
  auto* registry =
      companion::ShouldUseContextualLensPanelForImageSearch(&GetBrowser())
          ? SidePanelRegistry::Get(GetBrowserView()->GetActiveWebContents())
          : SidePanelCoordinator::GetGlobalSidePanelRegistry(&GetBrowser());
  if (registry == nullptr) {
    return;
  }

  auto* lens_side_panel_entry =
      registry->GetEntryForKey(SidePanelEntry::Key(SidePanelEntry::Id::kLens));
  if (lens_side_panel_entry == nullptr)
    return;

  lens_side_panel_entry->ResetIcon(ui::ImageModel::FromImage(favicon));
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
  if (companion::ShouldUseContextualLensPanelForImageSearch(&GetBrowser())) {
    auto* companion_helper = companion::CompanionTabHelper::FromWebContents(
        GetBrowserView()->GetActiveWebContents());
    return companion_helper->IsLensLaunchButtonEnabledForTesting();  // IN-TEST
  }

  DCHECK(lens_side_panel_view_);
  return lens_side_panel_view_->IsLaunchButtonEnabledForTesting();
}

bool LensSidePanelCoordinator::IsDefaultSearchProviderGoogle() {
  return search::DefaultSearchProviderIsGoogle(GetBrowser().profile());
}

std::u16string LensSidePanelCoordinator::GetComboboxLabel() {
  // If this panel was opened while the companion feature is enabled, then we
  // want this panel to be labelled like the companion panel.
  if (companion::ShouldUseContextualLensPanelForImageSearch(&GetBrowser())) {
    return l10n_util::GetStringUTF16(IDS_SIDE_PANEL_COMPANION_TITLE);
  }

  if (IsDefaultSearchProviderGoogle()) {
    return l10n_util::GetStringUTF16(IDS_GOOGLE_LENS_TITLE);
  }
  // Assuming not nullptr because side panel can't be opened if default search
  // provider is not initialized
  DCHECK(current_default_search_provider_);
  return current_default_search_provider_->image_search_branding_label();
}

const ui::ImageModel LensSidePanelCoordinator::GetFaviconImage() {
  // If this panel was opened while the companion feature is enabled, then we
  // want this panel to use the companion panel favicon.
  if (companion::ShouldUseContextualLensPanelForImageSearch(&GetBrowser())) {
    return ui::ImageModel::FromVectorIcon(
        features::IsChromeRefresh2023()
            ? vector_icons::
                  kGoogleSearchCompanionMonochromeLogoChromeRefreshIcon
            : vector_icons::kGoogleSearchCompanionMonochromeLogoIcon);
  }

  // If google is search engine, return checked-in lens icon.
  if (IsDefaultSearchProviderGoogle())
    return ui::ImageModel::FromVectorIcon(vector_icons::kGoogleLensLogoIcon);

  auto default_image = ui::ImageModel::FromVectorIcon(
      vector_icons::kImageSearchIcon, ui::kColorIcon);

  // Return default icon if the search provider is nullptr.
  if (template_url_service_ == nullptr ||
      template_url_service_->GetDefaultSearchProvider() == nullptr) {
    return default_image;
  }

  // Use favicon URL for image search favicon for 3P DSE to avoid latency in
  // showing the icon.
  auto url = template_url_service_->GetDefaultSearchProvider()->favicon_url();
  // Return default icon if the favicon url is empty.
  if (url.is_empty())
    return default_image;

  auto image = favicon_cache_->GetFaviconForIconUrl(
      url, base::BindRepeating(&LensSidePanelCoordinator::OnFaviconFetched,
                               weak_ptr_factory_.GetWeakPtr()));

  // Return default icon if the icon returned from cache is empty.
  return image.IsEmpty() ? std::move(default_image)
                         : ui::ImageModel::FromImage(image);
}

void LensSidePanelCoordinator::RegisterEntryAndShow(
    const content::OpenURLParams& params) {
  base::RecordAction(base::UserMetricsAction("LensUnifiedSidePanel.LensQuery"));
  const bool should_use_contextual_panel =
      companion::ShouldUseContextualLensPanelForImageSearch(&GetBrowser());
  auto* companion_helper = companion::CompanionTabHelper::FromWebContents(
      GetBrowserView()->GetActiveWebContents());
  auto* registry =
      should_use_contextual_panel
          ? SidePanelRegistry::Get(GetBrowserView()->GetActiveWebContents())
          : SidePanelCoordinator::GetGlobalSidePanelRegistry(&GetBrowser());

  // check if the view is already registered
  if (registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kLens)) != nullptr &&
      (lens_side_panel_view_ != nullptr || should_use_contextual_panel)) {
    // The user issued a follow-up Lens query.
    base::RecordAction(
        base::UserMetricsAction("LensUnifiedSidePanel.LensQuery_Followup"));
    if (should_use_contextual_panel) {
      companion_helper->OpenContextualLensView(params);
    } else {
      lens_side_panel_view_->OpenUrl(params);
    }
  } else {
    base::RecordAction(
        base::UserMetricsAction("LensUnifiedSidePanel.LensQuery_New"));
    if (should_use_contextual_panel) {
      // Side panel entry needs to be created and registered
      // in the companion side panel controller that exists per web contents in
      // order to prevent a dependency on views on CompanionTabHelper.
      companion_helper->CreateAndRegisterLensEntry(params, GetComboboxLabel(),
                                                   GetFaviconImage());
    } else {
      auto entry = std::make_unique<SidePanelEntry>(
          SidePanelEntry::Id::kLens, GetComboboxLabel(), GetFaviconImage(),
          base::BindRepeating(&LensSidePanelCoordinator::CreateLensWebView,
                              base::Unretained(this), params),
          base::BindRepeating(&LensSidePanelCoordinator::GetOpenInNewTabURL,
                              base::Unretained(this)));
      entry->AddObserver(this);
      registry->Register(std::move(entry));
    }
  }

  auto* side_panel_coordinator = GetSidePanelCoordinator();
  if (side_panel_coordinator->GetCurrentEntryId() !=
      SidePanelEntry::Id::kLens) {
    if (!side_panel_coordinator->IsSidePanelShowing()) {
      base::RecordAction(base::UserMetricsAction(
          "LensUnifiedSidePanel.LensQuery_SidePanelClosed"));
    } else {
      base::RecordAction(base::UserMetricsAction(
          "LensUnifiedSidePanel.LensQuery_SidePanelOpenNonLens"));
    }
    side_panel_coordinator->Show(SidePanelEntry::Id::kLens,
                                 SidePanelOpenTrigger::kLensContextMenu);
  } else {
    base::RecordAction(base::UserMetricsAction(
        "LensUnifiedSidePanel.LensQuery_SidePanelOpenLens"));
  }
}

content::WebContents* LensSidePanelCoordinator::GetViewWebContentsForTesting() {
  if (companion::ShouldUseContextualLensPanelForImageSearch(&GetBrowser())) {
    auto* companion_helper = companion::CompanionTabHelper::FromWebContents(
        GetBrowserView()->GetActiveWebContents());
    return companion_helper->GetLensViewWebContentsForTesting();  // IN-TEST
  }
  return lens_side_panel_view_ ? lens_side_panel_view_->GetWebContents()
                               : nullptr;
}

bool LensSidePanelCoordinator::OpenResultsInNewTabForTesting() {
  if (companion::ShouldUseContextualLensPanelForImageSearch(&GetBrowser())) {
    auto* companion_helper = companion::CompanionTabHelper::FromWebContents(
        GetBrowserView()->GetActiveWebContents());
    return companion_helper->OpenLensResultsInNewTabForTesting();  // IN-TEST
  }

  if (lens_side_panel_view_ == nullptr)
    return false;

  lens_side_panel_view_->LoadResultsInNewTab();
  return true;
}

std::unique_ptr<views::View> LensSidePanelCoordinator::CreateLensWebView(
    const content::OpenURLParams& params) {
  auto side_panel_view_ = std::make_unique<lens::LensUnifiedSidePanelView>(
      GetBrowserView(),
      base::BindRepeating(&LensSidePanelCoordinator::UpdateNewTabButtonState,
                          base::Unretained(this)));
  side_panel_view_->OpenUrl(params);
  lens_side_panel_view_ = side_panel_view_->GetWeakPtr();
  return side_panel_view_;
}

GURL LensSidePanelCoordinator::GetOpenInNewTabURL() const {
  // If our view is null, then pass an invalid URL to side panel coordinator.
  if (lens_side_panel_view_ == nullptr)
    return GURL();

  return lens_side_panel_view_->GetOpenInNewTabURL();
}

void LensSidePanelCoordinator::UpdateNewTabButtonState() {
  if (SidePanelCoordinator* side_panel_coordinator =
          GetSidePanelCoordinator()) {
    side_panel_coordinator->UpdateNewTabButtonState();
  }
}

BROWSER_USER_DATA_KEY_IMPL(LensSidePanelCoordinator);
