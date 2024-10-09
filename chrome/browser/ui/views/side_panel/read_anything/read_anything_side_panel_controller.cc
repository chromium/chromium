// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"

#include <algorithm>
#include <memory>

#include "base/check_is_test.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_service.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_page_handler.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/accessibility/reading/distillable_pages.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/common/locale_util.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_types.h"

using SidePanelWebUIViewT_ReadAnythingUntrustedUI =
    SidePanelWebUIViewT<ReadAnythingUntrustedUI>;
DECLARE_TEMPLATE_METADATA(SidePanelWebUIViewT_ReadAnythingUntrustedUI,
                          SidePanelWebUIViewT);

WEB_CONTENTS_USER_DATA_KEY_IMPL(ReadAnythingSidePanelControllerGlue);

ReadAnythingSidePanelControllerGlue::ReadAnythingSidePanelControllerGlue(
    content::WebContents* contents,
    ReadAnythingSidePanelController* controller)
    : content::WebContentsUserData<ReadAnythingSidePanelControllerGlue>(
          *contents),
      controller_(controller) {}

ReadAnythingSidePanelController::ReadAnythingSidePanelController(
    tabs::TabInterface* tab,
    SidePanelRegistry* side_panel_registry)
    : tab_(tab), side_panel_registry_(side_panel_registry) {
  CHECK(!side_panel_registry_->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything)));

  auto side_panel_entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kReadAnything,
      base::BindRepeating(&ReadAnythingSidePanelController::CreateContainerView,
                          base::Unretained(this)));
  side_panel_entry->AddObserver(this);
  side_panel_registry_->Register(std::move(side_panel_entry));

  tab_subscriptions_.push_back(tab_->RegisterWillDetach(
      base::BindRepeating(&ReadAnythingSidePanelController::TabWillDetach,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterDidEnterForeground(
      base::BindRepeating(&ReadAnythingSidePanelController::TabForegrounded,
                          weak_factory_.GetWeakPtr())));
  Observe(tab_->GetContents());

  // We do not know if the current tab is in the process of loading a page.
  // Assume that a page just finished loading to populate initial state.
  distillable_ = IsActivePageDistillable();
  UpdateIphVisibility();
}

ReadAnythingSidePanelController::~ReadAnythingSidePanelController() {
  if (web_view_) {
    web_view_->contents_wrapper()->web_contents()->RemoveUserData(
        ReadAnythingSidePanelControllerGlue::UserDataKey());
  }

  // Inform observers when |this| is destroyed so they can do their own cleanup.
  observers_.Notify(&ReadAnythingSidePanelController::Observer::
                        OnSidePanelControllerDestroyed);
}

void ReadAnythingSidePanelController::ResetForTabDiscard() {
  auto* current_entry = side_panel_registry_->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
  current_entry->RemoveObserver(this);
  side_panel_registry_->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
}

void ReadAnythingSidePanelController::AddPageHandlerAsObserver(
    base::WeakPtr<ReadAnythingUntrustedPageHandler> page_handler) {
  AddObserver(page_handler.get());
}

void ReadAnythingSidePanelController::RemovePageHandlerAsObserver(
    base::WeakPtr<ReadAnythingUntrustedPageHandler> page_handler) {
  RemoveObserver(page_handler.get());
}

void ReadAnythingSidePanelController::AddObserver(
    ReadAnythingSidePanelController::Observer* observer) {
  observers_.AddObserver(observer);
}

void ReadAnythingSidePanelController::RemoveObserver(
    ReadAnythingSidePanelController::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ReadAnythingSidePanelController::OnEntryShown(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kReadAnything);
  auto* service =
      ReadAnythingService::Get(tab_->GetBrowserWindowInterface()->GetProfile());
  // At the moment, services are created for normal and incognito profiles but
  // not unusual profile types. On the other hand,
  // ReadAnythingSidePanelController is created for all tabs. Thus we need a
  // nullptr check.
  if (service) {
    service->OnReadAnythingSidePanelEntryShown();
  }

  observers_.Notify(&ReadAnythingSidePanelController::Observer::Activate, true);
}

void ReadAnythingSidePanelController::OnEntryHidden(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kReadAnything);
  auto* service =
      ReadAnythingService::Get(tab_->GetBrowserWindowInterface()->GetProfile());
  // At the moment, services are created for normal and incognito profiles but
  // not unusual profile types. On the other hand,
  // ReadAnythingSidePanelController is created for all tabs. Thus we need a
  // nullptr check.
  if (service) {
    service->OnReadAnythingSidePanelEntryHidden();
  }
  observers_.Notify(&ReadAnythingSidePanelController::Observer::Activate,
                    false);
}

std::unique_ptr<views::View>
ReadAnythingSidePanelController::CreateContainerView() {
  // If there was an old WebView, clear the reference.
  if (web_view_) {
    web_view_->contents_wrapper()->web_contents()->RemoveUserData(
        ReadAnythingSidePanelControllerGlue::UserDataKey());
  }

  auto web_view = std::make_unique<ReadAnythingSidePanelWebView>(
      tab_->GetBrowserWindowInterface()->GetProfile());

  ReadAnythingSidePanelControllerGlue::CreateForWebContents(
      web_view->contents_wrapper()->web_contents(), this);
  web_view_ = web_view->GetWeakPtr();
  return std::move(web_view);
}

bool ReadAnythingSidePanelController::IsActivePageDistillable() const {
  auto url = tab_->GetContents()->GetLastCommittedURL();

  for (const std::string& distillable_domain : a11y::GetDistillableDomains()) {
    // If the url's domain is found in distillable domains AND the url has a
    // filename (i.e. it is not a home page or sub-home page), show the promo.
    if (url.DomainIs(distillable_domain) && !url.ExtractFileName().empty()) {
      return true;
    }
  }
  return false;
}

void ReadAnythingSidePanelController::TabForegrounded(tabs::TabInterface* tab) {
  UpdateIphVisibility();
}

void ReadAnythingSidePanelController::TabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  auto* coordinator =
      tab_->GetBrowserWindowInterface()->GetFeatures().side_panel_coordinator();
  // TODO(https://crbug.com/360163254): BrowserWithTestWindowTest currently does
  // not create a SidePanelCoordinator. This block will be unnecessary once that
  // changes.
  if (!coordinator) {
    CHECK_IS_TEST();
    return;  // IN-TEST
  }
  if (coordinator->IsSidePanelEntryShowing(
          SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything))) {
    coordinator->Close(/*suppress_animation=*/true);
  }
}

void ReadAnythingSidePanelController::DidStopLoading() {
  // The page finished loading.
  loading_ = false;
  UpdateIphVisibility();
}

void ReadAnythingSidePanelController::PrimaryPageChanged(content::Page& page) {
  // A navigation was committed but the page is still loading.
  previous_page_distillable_ = distillable_;
  loading_ = true;
  distillable_ = IsActivePageDistillable();
  UpdateIphVisibility();
}

void ReadAnythingSidePanelController::UpdateIphVisibility() {
  if (!tab_->IsInForeground()) {
    return;
  }

  bool should_show_iph = loading_ ? previous_page_distillable_ : distillable_;

  // Promo controller does not exist for incognito windows.
  auto* const user_ed =
      tab_->GetBrowserWindowInterface()->GetUserEducationInterface();

  if (should_show_iph) {
    user_ed->MaybeShowFeaturePromo(
        feature_engagement::kIPHReadingModeSidePanelFeature);
  } else {
    user_ed->AbortFeaturePromo(
        feature_engagement::kIPHReadingModeSidePanelFeature);
  }
}
