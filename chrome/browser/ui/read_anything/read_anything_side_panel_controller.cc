// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller.h"

#include <algorithm>
#include <climits>
#include <memory>
#include <optional>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/dom_distiller/tab_utils.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_omnibox_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/read_anything/read_anything_service.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_web_view.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/page_action/page_action_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_page_handler.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "chrome/grit/generated_resources.h"
#include "components/accessibility/reading/distillable_pages.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/common/locale_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/url_constants.h"
#include "read_anything_entry_point_controller.h"
#include "read_anything_side_panel_controller.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
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
    SidePanelRegistry* side_panel_registry,
    content::WebContents* web_contents)
    : tab_(tab), side_panel_registry_(side_panel_registry) {
  CHECK(!side_panel_registry_->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything)));

  auto side_panel_entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything),
      base::BindRepeating(&ReadAnythingSidePanelController::CreateContainerView,
                          base::Unretained(this)),
      base::BindRepeating(
          &ReadAnythingSidePanelController::GetPreferredDefaultWidth,
          base::Unretained(this)));
  side_panel_entry->AddObserver(this);
  side_panel_registry_->Register(std::move(side_panel_entry));

  tab_subscriptions_.push_back(tab_->RegisterWillDetach(
      base::BindRepeating(&ReadAnythingSidePanelController::TabWillDetach,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterDidActivate(
      base::BindRepeating(&ReadAnythingSidePanelController::TabForegrounded,
                          weak_factory_.GetWeakPtr())));
  Observe(web_contents);

  // We do not know if the current tab is in the process of loading a page.
  // Assume that a page just finished loading to populate initial state.
  distillable_ = IsActivePageDistillable();
  UpdateIphVisibility();
  if (features::IsReadAnythingOmniboxChipEnabled() &&
      base::FeatureList::IsEnabled(features::kPageActionsMigration) &&
      !features::IsImmersiveReadAnythingEnabled()) {
    // The omnibox controller can't add itself as an observer because it needs
    // to access this controller during its construction, so add it as an
    // observer here.
    omnibox_controller_ = std::make_unique<ReadAnythingOmniboxController>(tab_);
    AddObserver(omnibox_controller_.get());
  }
}

ReadAnythingSidePanelController::~ReadAnythingSidePanelController() {
  if (web_view_ && web_view_->contents_wrapper()) {
    web_view_->contents_wrapper()->web_contents()->RemoveUserData(
        ReadAnythingSidePanelControllerGlue::UserDataKey());
  }
  if (omnibox_controller_) {
    RemoveObserver(omnibox_controller_.get());
    omnibox_controller_.reset();
  }

  // Inform observers when |this| is destroyed so they can do their own cleanup.
  observers_.Notify(&Observer::OnDestroyed);
}

void ReadAnythingSidePanelController::ResetForTabDiscard() {
  auto* current_entry = side_panel_registry_->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
  current_entry->RemoveObserver(this);
  side_panel_registry_->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
}

void ReadAnythingSidePanelController::RemoveReadAnythingControllerGlue() {
  if (web_view_ && web_view_->contents_wrapper()) {
    web_view_->contents_wrapper()->web_contents()->RemoveUserData(
        ReadAnythingControllerGlue::UserDataKey());
  }
}

void ReadAnythingSidePanelController::AddPageHandlerAsObserver(
    base::WeakPtr<ReadAnythingUntrustedPageHandler> page_handler) {
  AddObserver(page_handler.get());
}

void ReadAnythingSidePanelController::RemovePageHandlerAsObserver(
    base::WeakPtr<ReadAnythingUntrustedPageHandler> page_handler) {
  RemoveObserver(page_handler.get());
}

void ReadAnythingSidePanelController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ReadAnythingSidePanelController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ReadAnythingSidePanelController::OnEntryShown(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kReadAnything);
  if (!features::IsImmersiveReadAnythingEnabled()) {
    auto* service = ReadAnythingService::Get(
        tab_->GetBrowserWindowInterface()->GetProfile());
    // At the moment, services are created for normal, incognito, and guest
    // profiles but not unusual profile types. On the other hand,
    // ReadAnythingSidePanelController is created for all tabs. Thus we need a
    // nullptr check.
    if (service) {
      service->OnReadAnythingShown();
    }
  }

  // Build and record UKM record for SidePanelShown to true on the current
  // source Id
  std::optional<SidePanelOpenTrigger> open_trigger = entry->last_open_trigger();
  std::optional<ReadAnythingOpenTrigger> read_anything_trigger =
      open_trigger.has_value()
          ? read_anything::SidePanelToReadAnythingOpenTrigger(
                open_trigger.value())
          : std::optional<ReadAnythingOpenTrigger>();
  if (auto* contents = web_contents()) {
    if (content::RenderFrameHost* main_frame =
            contents->GetPrimaryMainFrame()) {
      ukm::SourceId source_id = main_frame->GetPageUkmSourceId();
      ukm::builders::Accessibility_ReadAnything_SidePanel builder(source_id);
      builder.SetShown(true);

      if (open_trigger.has_value()) {
        builder.SetEntryPoint(static_cast<int64_t>(open_trigger.value()));
      }
      builder.Record(ukm::UkmRecorder::Get());
    }
  }

  if (features::IsImmersiveReadAnythingEnabled()) {
    auto* controller = ReadAnythingController::From(tab_);
    CHECK(controller);
    controller->OnEntryShown(read_anything_trigger);
  } else {
    observers_.Notify(&Observer::Activate, true, read_anything_trigger);
  }
}

void ReadAnythingSidePanelController::OnEntryHidden(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kReadAnything);

  // Build and record UKM record for SidePanelClosed to true on the current
  // source id
  if (web_contents()) {
    if (content::RenderFrameHost* main_frame =
            web_contents()->GetPrimaryMainFrame()) {
      ukm::SourceId source_id = main_frame->GetPageUkmSourceId();
      ukm::builders::Accessibility_ReadAnything_SidePanel(source_id)
          .SetClosed(true)
          .Record(ukm::UkmRecorder::Get());
    }
  }

  if (!features::IsImmersiveReadAnythingEnabled()) {
    auto* service = ReadAnythingService::Get(
        tab_->GetBrowserWindowInterface()->GetProfile());
    // At the moment, services are created for normal, guest, and incognito
    // profiles but not unusual profile types. On the other hand,
    // ReadAnythingSidePanelController is created for all tabs. Thus we need a
    // nullptr check.
    if (service) {
      service->OnReadAnythingHidden();
    }
  }

  if (features::IsImmersiveReadAnythingEnabled()) {
    auto* controller = ReadAnythingController::From(tab_);
    CHECK(controller);
    controller->OnEntryHidden();
  } else {
    observers_.Notify(&Observer::Activate, false,
                      std::optional<ReadAnythingOpenTrigger>());
  }
}

void ReadAnythingSidePanelController::OnEntryWillHide(
    SidePanelEntry* entry,
    SidePanelEntryHideReason reason) {
  if (reason == SidePanelEntryHideReason::kSidePanelClosed) {
    ReturnWebUIToController();
  }
}

void ReadAnythingSidePanelController::ReturnWebUIToController() {
  if (!features::IsImmersiveReadAnythingEnabled()) {
    return;
  }
  if (!web_view_ || !web_view_->contents_wrapper()) {
    return;
  }
  web_view_->contents_wrapper()->web_contents()->RemoveUserData(
      ReadAnythingSidePanelControllerGlue::UserDataKey());
  auto* controller = ReadAnythingController::From(tab_);
  CHECK(controller);
  controller->TransferWebUiOwnership(web_view_->TakeContentsWrapper());
}

std::unique_ptr<views::View>
ReadAnythingSidePanelController::CreateContainerView(
    SidePanelEntryScope& scope) {
  // If there was an old WebView, clear the reference.
  if (web_view_ && web_view_->contents_wrapper()) {
    web_view_->contents_wrapper()->web_contents()->RemoveUserData(
        ReadAnythingSidePanelControllerGlue::UserDataKey());
  }

  std::unique_ptr<ReadAnythingSidePanelWebView> web_view;
  if (features::IsImmersiveReadAnythingEnabled()) {
    web_view = std::make_unique<ReadAnythingSidePanelWebView>(
        tab_->GetBrowserWindowInterface()->GetProfile(), scope,
        ReadAnythingController::From(tab_)->GetOrCreateWebUIWrapper(
            ReadAnythingController::PresentationState::kInSidePanel));
  } else {
    web_view = std::make_unique<ReadAnythingSidePanelWebView>(
        tab_->GetBrowserWindowInterface()->GetProfile(), scope);
  }
  ReadAnythingSidePanelControllerGlue::CreateForWebContents(
      web_view->contents_wrapper()->web_contents(), this);
  web_view_ = web_view->GetWeakPtr();
  return std::move(web_view);
}

int ReadAnythingSidePanelController::GetPreferredDefaultWidth() {
  // Use 50% of the current WebView width
  return BrowserElementsViews::From(tab_->GetBrowserWindowInterface())
             ->RetrieveView(kActiveContentsWebViewRetrievalId)
             ->GetContentsBounds()
             .width() /
         2;
}

bool ReadAnythingSidePanelController::IsActivePageDistillable() const {
  auto url = web_contents()->GetLastCommittedURL();

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
  if (!features::IsImmersiveReadAnythingEnabled()) {
    observers_.Notify(&Observer::OnTabWillDetach);
  }

  if (!tab_->IsActivated()) {
    return;
  }
  auto* browser_window_interface = tab_->GetBrowserWindowInterface();
  if (!browser_window_interface) {
    return;
  }
  auto* const side_panel_ui =
      browser_window_interface->GetFeatures().side_panel_ui();
  // TODO(https://crbug.com/360163254): BrowserWithTestWindowTest currently does
  // not create a SidePanelCoordinator. This block will be unnecessary once that
  // changes.
  if (!side_panel_ui) {
    // TODO(webium): create a SidePanelCoordinator for WebUIBrowser.
    // This is a temporary solution to avoid a crash.
    if (!webui_browser::IsWebUIBrowserEnabled()) {
      CHECK_IS_TEST();
    }
    return;  // IN-TEST
  }

  SidePanelEntry::Key read_anything_key(SidePanelEntry::Id::kReadAnything);
  if (side_panel_ui->IsSidePanelEntryShowing(read_anything_key)) {
    SidePanelEntry* const entry =
        side_panel_registry_->GetEntryForKey(read_anything_key);
    CHECK(entry);
    side_panel_ui->Close(entry->type(),
                         SidePanelEntryHideReason::kSidePanelClosed,
                         /*suppress_animations=*/true);
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
  if (!tab_->IsActivated()) {
    return;
  }

  bool should_show_iph = loading_ ? previous_page_distillable_ : distillable_;

  // Promo controller does not exist for incognito windows.
  auto* const user_ed =
      BrowserUserEducationInterface::From(tab_->GetBrowserWindowInterface());

  if (should_show_iph) {
    user_ed->MaybeShowFeaturePromo(
        feature_engagement::kIPHReadingModeSidePanelFeature);
  } else {
    user_ed->AbortFeaturePromo(
        feature_engagement::kIPHReadingModeSidePanelFeature);
  }
}

void ReadAnythingSidePanelController::SetDwellTimeForTesting(
    base::TimeTicks test_time) {
  if (omnibox_controller_) {
    omnibox_controller_->SetDwellTimeForTesting(test_time);
  }
}
