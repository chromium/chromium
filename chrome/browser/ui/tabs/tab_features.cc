// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/public/tab_features.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/autofill_prediction_improvements/chrome_autofill_prediction_improvements_client.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"
#include "chrome/browser/fingerprinting_protection/chrome_fingerprinting_protection_web_contents_helper_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_tab_observer.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/side_panel_controller_views.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_controller.h"
#include "chrome/browser/user_annotations/user_annotations_web_contents_observer.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/permissions/permission_indicators_tab_data.h"

namespace tabs {

namespace {

// This is the generic entry point for test code to stub out TabFeature
// functionality. It is called by production code, but only used by tests.
TabFeatures::TabFeaturesFactory& GetFactory() {
  static base::NoDestructor<TabFeatures::TabFeaturesFactory> factory;
  return *factory;
}

}  // namespace

// static
std::unique_ptr<TabFeatures> TabFeatures::CreateTabFeatures() {
  if (GetFactory()) {
    return GetFactory().Run();
  }
  // Constructor is protected.
  return base::WrapUnique(new TabFeatures());
}

TabFeatures::~TabFeatures() = default;

// static
void TabFeatures::ReplaceTabFeaturesForTesting(TabFeaturesFactory factory) {
  TabFeatures::TabFeaturesFactory& f = GetFactory();
  f = std::move(factory);
}

void TabFeatures::Init(TabInterface& tab, Profile* profile) {
  CHECK(!initialized_);
  initialized_ = true;

  CHECK(tab.GetBrowserWindowInterface());

  tab_subscriptions_.push_back(
      tab.RegisterWillDiscardContents(base::BindRepeating(
          &TabFeatures::WillDiscardContents, weak_factory_.GetWeakPtr())));

  // TODO(crbug.com/346148554): Do not create a SidePanelRegistry or
  // dependencies for non-normal browsers.
  side_panel_registry_ = std::make_unique<SidePanelRegistry>(&tab);

  // Features that are only enabled for normal browser windows. By default most
  // features should be instantiated in this block.
  if (tab.IsInNormalWindow()) {
    lens_overlay_controller_ = CreateLensController(&tab, profile);

    // Each time a new tab is created, validate the topics calculation schedule
    // to help investigate a scheduling bug (crbug.com/343750866).
    if (browsing_topics::BrowsingTopicsService* browsing_topics_service =
            browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(
                profile)) {
      browsing_topics_service->ValidateCalculationSchedule();
    }

    permission_indicators_tab_data_ =
        std::make_unique<permissions::PermissionIndicatorsTabData>(
            tab.GetContents());

    user_annotations_web_contents_observer_ =
        user_annotations::UserAnnotationsWebContentsObserver::
            MaybeCreateForWebContents(tab.GetContents());

    chrome_autofill_prediction_improvements_client_ =
        ChromeAutofillPredictionImprovementsClient::MaybeCreateForWebContents(
            tab.GetContents());

    if (!profile->IsIncognitoProfile()) {
      commerce_ui_tab_helper_ =
          CreateCommerceUiTabHelper(tab.GetContents(), profile);
    }

    privacy_sandbox_tab_observer_ =
        std::make_unique<privacy_sandbox::PrivacySandboxTabObserver>(
            tab.GetContents());
  }

  if (web_app::AreWebAppsEnabled(profile)) {
    auto* web_app_tab_helper =
        web_app::WebAppTabHelper::FromWebContents(tab.GetContents());
    web_app_tab_helper->InitForTabFeatures(&tab);
  }

  // FedCM is supported in general web content, but not in chrome UI. Of the
  // BrowserWindow types, devtools show Chrome UI and the rest show general web
  // content.
  if (tab.GetBrowserWindowInterface()->GetType() !=
      BrowserWindowInterface::Type::TYPE_DEVTOOLS) {
    fedcm_account_selection_view_controller_ =
        std::make_unique<FedCmAccountSelectionViewController>(&tab);
  }

  customize_chrome_side_panel_controller_ =
      std::make_unique<customize_chrome::SidePanelControllerViews>(tab);

  extension_side_panel_manager_ =
      std::make_unique<extensions::ExtensionSidePanelManager>(
          profile, tab.GetContents(), side_panel_registry_.get());

  data_protection_controller_ = std::make_unique<
      enterprise_data_protection::DataProtectionNavigationController>(&tab);

  // TODO(https://crbug.com/355485153): Move this into the normal window block.
  read_anything_side_panel_controller_ =
      std::make_unique<ReadAnythingSidePanelController>(
          &tab, side_panel_registry_.get());

  if (fingerprinting_protection_filter::features::
          IsFingerprintingProtectionFeatureEnabled()) {
    CreateFingerprintingProtectionWebContentsHelper(
        tab.GetContents(), profile->GetPrefs(),
        TrackingProtectionSettingsFactory::GetForProfile(profile),
        profile->IsIncognitoProfile());
  }
}

TabFeatures::TabFeatures() = default;

std::unique_ptr<LensOverlayController> TabFeatures::CreateLensController(
    TabInterface* tab,
    Profile* profile) {
  return std::make_unique<LensOverlayController>(
      tab, profile->GetVariationsClient(),
      IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs(),
      SyncServiceFactory::GetForProfile(profile),
      ThemeServiceFactory::GetForProfile(profile));
}

std::unique_ptr<commerce::CommerceUiTabHelper>
TabFeatures::CreateCommerceUiTabHelper(content::WebContents* web_contents,
                                       Profile* profile) {
  // TODO(crbug.com/40863325): Consider using the in-memory cache instead.
  return std::make_unique<commerce::CommerceUiTabHelper>(
      web_contents,
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile),
      BookmarkModelFactory::GetForBrowserContext(profile),
      ImageFetcherServiceFactory::GetForKey(profile->GetProfileKey())
          ->GetImageFetcher(image_fetcher::ImageFetcherConfig::kNetworkOnly),
      side_panel_registry_.get());
}

void TabFeatures::WillDiscardContents(tabs::TabInterface* tab,
                                      content::WebContents* old_contents,
                                      content::WebContents* new_contents) {
  // This method is transiently used to reset features that do not handle tab
  // discarding themselves.
  read_anything_side_panel_controller_->ResetForTabDiscard();
  read_anything_side_panel_controller_.reset();
  read_anything_side_panel_controller_ =
      std::make_unique<ReadAnythingSidePanelController>(
          tab, side_panel_registry_.get());

  // Deregister side-panel entries that are web-contents scoped rather than tab
  // scoped.
  side_panel_registry_->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kAboutThisSite));
  extension_side_panel_manager_->WillDiscard();
  extension_side_panel_manager_ =
      std::make_unique<extensions::ExtensionSidePanelManager>(
          Profile::FromBrowserContext(new_contents->GetBrowserContext()),
          new_contents, side_panel_registry_.get());

  if (commerce_ui_tab_helper_) {
    commerce_ui_tab_helper_.reset();
    commerce_ui_tab_helper_ = CreateCommerceUiTabHelper(
        new_contents,
        Profile::FromBrowserContext(new_contents->GetBrowserContext()));
  }
  if (user_annotations_web_contents_observer_) {
    user_annotations_web_contents_observer_ =
        user_annotations::UserAnnotationsWebContentsObserver::
            MaybeCreateForWebContents(new_contents);
  }
  if (chrome_autofill_prediction_improvements_client_) {
    chrome_autofill_prediction_improvements_client_ =
        ChromeAutofillPredictionImprovementsClient::MaybeCreateForWebContents(
            new_contents);
  }

  if (privacy_sandbox_tab_observer_) {
    privacy_sandbox_tab_observer_.reset();
    privacy_sandbox_tab_observer_ =
        std::make_unique<privacy_sandbox::PrivacySandboxTabObserver>(
            tab->GetContents());
  }
}

}  // namespace tabs
