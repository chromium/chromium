// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller.h"

#include <memory>
#include <optional>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/read_anything/read_anything_contents_wrapper.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_web_view.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

ReadAnythingSidePanelController::ReadAnythingSidePanelController(
    tabs::TabInterface* tab,
    SidePanelRegistry* side_panel_registry)
    : tab_(tab) {
  CHECK(!side_panel_registry->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything)));

  auto side_panel_entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything),
      base::BindRepeating(&ReadAnythingSidePanelController::CreateContainerView,
                          base::Unretained(this)),
      base::BindRepeating(
          &ReadAnythingSidePanelController::GetPreferredDefaultWidth,
          base::Unretained(this)));
  side_panel_entry->AddObserver(this);
  side_panel_registry->Register(std::move(side_panel_entry));

  tab_subscriptions_.push_back(tab_->RegisterWillDetach(
      base::BindRepeating(&ReadAnythingSidePanelController::TabWillDetach,
                          weak_factory_.GetWeakPtr())));
}

ReadAnythingSidePanelController::~ReadAnythingSidePanelController() = default;

void ReadAnythingSidePanelController::RemoveReadAnythingControllerGlue() {
  if (web_view_ && web_view_->contents_wrapper()) {
    web_view_->contents_wrapper()->web_contents()->RemoveUserData(
        ReadAnythingControllerGlue::UserDataKey());
  }
}

void ReadAnythingSidePanelController::OnEntryShown(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kReadAnything);

  // Build and record UKM record for SidePanelShown to true on the current
  // source Id
  std::optional<SidePanelOpenTrigger> open_trigger = entry->last_open_trigger();
  ReadAnythingOpenTrigger read_anything_trigger =
      open_trigger.has_value()
          ? read_anything::SidePanelToReadAnythingOpenTrigger(
                open_trigger.value())
          : ReadAnythingOpenTrigger::kUnknown;
  if (content::WebContents* contents = tab_->GetContents()) {
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

  auto* controller = ReadAnythingController::From(tab_);
  CHECK(controller);
  controller->OnEntryShown(read_anything_trigger);
}

void ReadAnythingSidePanelController::OnEntryHidden(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kReadAnything);

  // Build and record UKM record for SidePanelClosed to true on the current
  // source id
  if (content::WebContents* contents = tab_->GetContents()) {
    if (content::RenderFrameHost* main_frame =
            contents->GetPrimaryMainFrame()) {
      ukm::SourceId source_id = main_frame->GetPageUkmSourceId();
      ukm::builders::Accessibility_ReadAnything_SidePanel(source_id)
          .SetClosed(true)
          .Record(ukm::UkmRecorder::Get());
    }
  }

  auto* controller = ReadAnythingController::From(tab_);
  CHECK(controller);
  controller->OnEntryHidden();

  // When the reading mode side panel is replaced with another side panel,
  // ownership of its WebContents is transferred back to the
  // ReadAnythingController in OnEntryWillHide(). If the reading mode side
  // panel is later reopened, it would attempt to use the previously cached
  // view, which now lacks a valid WebContents, causing a crash. Clearing the
  // cached view prevents this.
  if (should_clear_cached_view_on_hidden_) {
    entry->ClearCachedView();
    should_clear_cached_view_on_hidden_ = false;
  }
}

void ReadAnythingSidePanelController::OnEntryWillHide(
    SidePanelEntry* entry,
    SidePanelEntryHideReason reason) {
  auto* controller = ReadAnythingController::From(tab_);
  CHECK(controller);
  controller->OnSidePanelWillHide(reason);

  if (reason == SidePanelEntryHideReason::kSidePanelClosed ||
      reason == SidePanelEntryHideReason::kReplaced) {
    ReturnWebUIToController();
  }
  if (reason == SidePanelEntryHideReason::kReplaced) {
    should_clear_cached_view_on_hidden_ = true;
  }
}

void ReadAnythingSidePanelController::ReturnWebUIToController() {
  if (!web_view_ || !web_view_->contents_wrapper()) {
    return;
  }
  auto* controller = ReadAnythingController::From(tab_);
  CHECK(controller);
  controller->TransferWebUiOwnership(
      ReadAnythingContentsWrapper(web_view_->TakeContentsWrapper()),
      ReadAnythingController::PresentationState::kInSidePanel);
}

std::unique_ptr<views::View>
ReadAnythingSidePanelController::CreateContainerView(
    SidePanelEntryScope& scope) {
  std::unique_ptr<ReadAnythingSidePanelWebView> web_view =
      std::make_unique<ReadAnythingSidePanelWebView>(
          scope,
          ReadAnythingController::From(tab_)
              ->GetOrCreateWebUIWrapper(
                  ReadAnythingController::PresentationState::kInSidePanel)
              .release());
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

void ReadAnythingSidePanelController::TabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  if (!tab_->IsActivated()) {
    return;
  }
  auto* browser_window_interface = tab_->GetBrowserWindowInterface();
  if (!browser_window_interface) {
    return;
  }
  auto* const side_panel_ui = SidePanelUI::From(browser_window_interface);
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
    side_panel_ui->Close(SidePanelEntryHideReason::kSidePanelClosed,
                         /*suppress_animations=*/true);
  }
}
