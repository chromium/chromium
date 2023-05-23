// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_page_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"
#include "content/public/browser/web_ui.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_tree_update.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#endif

using read_anything::mojom::ReadAnythingTheme;
using read_anything::mojom::UntrustedPage;
using read_anything::mojom::UntrustedPageHandler;

ReadAnythingPageHandler::ReadAnythingPageHandler(
    mojo::PendingRemote<UntrustedPage> page,
    mojo::PendingReceiver<UntrustedPageHandler> receiver,
    content::WebUI* web_ui)
    : browser_(chrome::FindLastActive()),
      web_ui_(web_ui),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  DCHECK(browser_);
  browser_->tab_strip_model()->AddObserver(this);
  ax_action_handler_observer_.Observe(
      ui::AXActionHandlerRegistry::GetInstance());

  coordinator_ = ReadAnythingCoordinator::FromBrowser(browser_);
  if (coordinator_) {
    coordinator_->AddObserver(this);
    coordinator_->AddModelObserver(this);
  }

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (features::IsReadAnythingWithScreen2xEnabled()) {
    if (screen_ai::ScreenAIInstallState::GetInstance()->get_state() ==
        screen_ai::ScreenAIInstallState::State::kReady) {
      // Notify that the screen ai service is already ready so we can bind to
      // the content extractor.
      page_->ScreenAIServiceReady();
    } else if (!component_ready_observer_.IsObserving()) {
      component_ready_observer_.Observe(
          screen_ai::ScreenAIInstallState::GetInstance());
    }
  }
#endif
  OnActiveWebContentsChanged();
}

ReadAnythingPageHandler::~ReadAnythingPageHandler() {
  TabStripModelObserver::StopObservingAll(this);
  Observe(nullptr);

  if (!coordinator_)
    return;

  // If |this| is destroyed before the |ReadAnythingCoordinator|, then remove
  // |this| from the observer lists. In the cases where the coordinator is
  // destroyed first, these will have been destroyed before this call.
  coordinator_->RemoveObserver(this);
  coordinator_->RemoveModelObserver(this);
}

///////////////////////////////////////////////////////////////////////////////
// ui::AXActionHandlerObserver:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingPageHandler::TreeRemoved(ui::AXTreeID ax_tree_id) {
  page_->OnAXTreeDestroyed(ax_tree_id);
}

///////////////////////////////////////////////////////////////////////////////
// read_anything::mojom::UntrustedPageHandler:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingPageHandler::OnCopy() {
  web_contents()->Copy();
}

void ReadAnythingPageHandler::OnLinkClicked(const ui::AXTreeID& target_tree_id,
                                            ui::AXNodeID target_node_id) {
  ui::AXActionData action_data;
  action_data.target_tree_id = target_tree_id;
  action_data.action = ax::mojom::Action::kDoDefault;
  action_data.target_node_id = target_node_id;
  ui::AXActionHandlerBase* handler =
      ui::AXActionHandlerRegistry::GetInstance()->GetActionHandler(
          target_tree_id);
  if (!handler) {
    return;
  }
  handler->PerformAction(action_data);
}

void ReadAnythingPageHandler::OnSelectionChange(
    const ui::AXTreeID& target_tree_id,
    ui::AXNodeID anchor_node_id,
    int anchor_offset,
    ui::AXNodeID focus_node_id,
    int focus_offset) {
  ui::AXActionData action_data;
  action_data.target_tree_id = target_tree_id;
  action_data.action = ax::mojom::Action::kSetSelection;
  action_data.anchor_node_id = anchor_node_id;
  action_data.anchor_offset = anchor_offset;
  action_data.focus_node_id = focus_node_id;
  action_data.focus_offset = focus_offset;
  ui::AXActionHandlerBase* handler =
      ui::AXActionHandlerRegistry::GetInstance()->GetActionHandler(
          target_tree_id);
  if (!handler) {
    return;
  }
  handler->PerformAction(action_data);
}

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingModel::Observer:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingPageHandler::OnReadAnythingThemeChanged(
    const std::string& font_name,
    double font_scale,
    ui::ColorId foreground_color_id,
    ui::ColorId background_color_id,
    ui::ColorId separator_color_id,
    ui::ColorId dropdown_color_id,
    ui::ColorId selected_dropdown_color_id,
    ui::ColorId focus_ring_color_id,
    read_anything::mojom::LineSpacing line_spacing,
    read_anything::mojom::LetterSpacing letter_spacing) {
  // Elsewhere in this file, `web_contents` refers to the active web contents
  // in the tab strip. In this case, `web_contents` refers to the web contents
  // hosting the WebUI.
  content::WebContents* web_contents = web_ui_->GetWebContents();
  SkColor foreground_skcolor =
      web_contents->GetColorProvider().GetColor(foreground_color_id);
  SkColor background_skcolor =
      web_contents->GetColorProvider().GetColor(background_color_id);

  page_->OnThemeChanged(
      ReadAnythingTheme::New(font_name, font_scale, foreground_skcolor,
                             background_skcolor, line_spacing, letter_spacing));
}

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingCoordinator::Observer:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingPageHandler::Activate(bool active) {
  active_ = active;
  OnActiveWebContentsChanged();
}

void ReadAnythingPageHandler::OnCoordinatorDestroyed() {
  coordinator_ = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// screen_ai::ScreenAIInstallState::Observer:
///////////////////////////////////////////////////////////////////////////////

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void ReadAnythingPageHandler::StateChanged(
    screen_ai::ScreenAIInstallState::State state) {
  DCHECK(features::IsReadAnythingWithScreen2xEnabled());
  // If Screen AI library is downloaded but not initialized yet, ensure it is
  // loadable and initializes without any problems.
  if (state == screen_ai::ScreenAIInstallState::State::kDownloaded) {
    screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(
        browser_->profile())
        ->InitializeMainContentExtractionIfNeeded();
    return;
  }
  if (state == screen_ai::ScreenAIInstallState::State::kReady) {
    page_->ScreenAIServiceReady();
  }
}
#endif

///////////////////////////////////////////////////////////////////////////////
// TabStripModelObserver:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingPageHandler::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    OnActiveWebContentsChanged();
  }
}

void ReadAnythingPageHandler::OnTabStripModelDestroyed(
    TabStripModel* tab_strip_model) {
  // If the TabStripModel is destroyed before |this|, remove |this| as an
  // observer.
  DCHECK(browser_);
  tab_strip_model->RemoveObserver(this);
}

///////////////////////////////////////////////////////////////////////////////
// content::WebContentsObserver:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingPageHandler::PrimaryPageChanged(content::Page& page) {
  OnActiveAXTreeIDChanged();
}

void ReadAnythingPageHandler::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& details) {
  page_->AccessibilityEventReceived(details.ax_tree_id, details.updates,
                                    details.events);
}

///////////////////////////////////////////////////////////////////////////////

void ReadAnythingPageHandler::OnActiveWebContentsChanged() {
  // TODO(crbug.com/1266555): Disable accessibility.and stop observing events
  // on the now inactive tab. But make sure that we don't disable it for
  // assistive technology users. Some options here are:
  // 1. Cache the current AXMode of the active web contents before enabling
  //    accessibility, and reset the mode to that mode when the tab becomes
  //    inactive.
  // 2. Set an AXContext on the web contents with web contents only mode
  //    enabled.
  content::WebContents* web_contents = nullptr;
  if (active_) {
    web_contents = browser_->tab_strip_model()->GetActiveWebContents();
  }
  Observe(web_contents);
  // Enable accessibility for the top level render frame and all descendants.
  // This causes AXTreeSerializer to reset and send accessibility events of
  // the AXTree when it is re-serialized.
  // TODO(crbug.com/1266555): Only enable kReadAnythingAXMode while still
  // causing the reset.
  if (web_contents) {
    web_contents->EnableWebContentsOnlyAccessibilityMode();
  }
  OnActiveAXTreeIDChanged();
}

void ReadAnythingPageHandler::OnActiveAXTreeIDChanged() {
  ui::AXTreeID tree_id = ui::AXTreeIDUnknown();
  ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
  GURL visible_url;
  if (active_ && web_contents()) {
    visible_url = web_contents()->GetVisibleURL();
    content::RenderFrameHost* render_frame_host =
        web_contents()->GetPrimaryMainFrame();
    if (render_frame_host) {
      tree_id = render_frame_host->GetAXTreeID();
      ukm_source_id = render_frame_host->GetPageUkmSourceId();
    }
  }

  page_->OnActiveAXTreeIDChanged(tree_id, ukm_source_id, visible_url);
}
