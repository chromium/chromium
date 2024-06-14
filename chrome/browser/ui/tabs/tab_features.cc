// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/public/tab_features.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "components/browsing_topics/browsing_topics_service.h"
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

void TabFeatures::Init(TabInterface* tab, Profile* profile) {
  CHECK(!initialized_);
  initialized_ = true;

  // Features that are only enabled for normal browser windows. By default most
  // features should be instantiated in this block.
  if (tab->IsInNormalWindow()) {
    lens_overlay_controller_ = CreateLensController(tab, profile);

    // Each time a new tab is created, validate the topics calculation schedule
    // to help investigate a scheduling bug (crbug.com/343750866).
    if (browsing_topics::BrowsingTopicsService* browsing_topics_service =
            browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(
                profile)) {
      browsing_topics_service->ValidateCalculationSchedule();
    }

    permission_indicators_tab_data_ =
        std::make_unique<permissions::PermissionIndicatorsTabData>(
            tab->GetContents());
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

}  // namespace tabs
