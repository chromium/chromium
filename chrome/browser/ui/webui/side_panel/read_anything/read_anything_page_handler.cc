// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_page_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"
#include "content/public/browser/web_ui.h"
#include "ui/accessibility/ax_tree_update.h"

using read_anything::mojom::Page;
using read_anything::mojom::PageHandler;
using read_anything::mojom::ReadAnythingTheme;

ReadAnythingPageHandler::ReadAnythingPageHandler(
    mojo::PendingRemote<Page> page,
    mojo::PendingReceiver<PageHandler> receiver,
    content::WebUI* web_ui)
    : browser_(chrome::FindLastActive()),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_ui_(web_ui) {
  // Register |this| as a |ReadAnythingModel::Observer| with the coordinator
  // for the component. This will allow the IPC to update the front-end web ui.

  if (!browser_)
    return;

  coordinator_ = ReadAnythingCoordinator::FromBrowser(browser_);
  if (coordinator_) {
    coordinator_->AddObserver(this);
    coordinator_->AddModelObserver(this);
  }

  delegate_ = static_cast<ReadAnythingPageHandler::Delegate*>(
      coordinator_->GetController());
  if (delegate_)
    delegate_->OnUIReady();
}

ReadAnythingPageHandler::~ReadAnythingPageHandler() {
  if (!coordinator_)
    return;

  // If |this| is destroyed before the |ReadAnythingCoordinator|, then remove
  // |this| from the observer lists. In the cases where the coordinator is
  // destroyed first, these will have been destroyed before this call.
  coordinator_->RemoveObserver(this);
  coordinator_->RemoveModelObserver(this);

  delegate_ = static_cast<ReadAnythingPageHandler::Delegate*>(
      coordinator_->GetController());
  if (delegate_)
    delegate_->OnUIDestroyed();
}

void ReadAnythingPageHandler::OnCoordinatorDestroyed() {
  coordinator_ = nullptr;
  delegate_ = nullptr;
}

void ReadAnythingPageHandler::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& details) {
  page_->AccessibilityEventReceived(details.ax_tree_id, details.updates,
                                    details.events);
}

void ReadAnythingPageHandler::OnActiveAXTreeIDChanged(
    const ui::AXTreeID& tree_id,
    const ukm::SourceId& ukm_source_id) {
  page_->OnActiveAXTreeIDChanged(tree_id, ukm_source_id);
}

void ReadAnythingPageHandler::OnAXTreeDestroyed(const ui::AXTreeID& tree_id) {
  page_->OnAXTreeDestroyed(tree_id);
}

void ReadAnythingPageHandler::OnReadAnythingThemeChanged(
    const std::string& font_name,
    double font_scale,
    ui::ColorId foreground_color_id,
    ui::ColorId background_color_id,
    ui::ColorId separator_color_id,
    read_anything::mojom::LineSpacing line_spacing,
    read_anything::mojom::LetterSpacing letter_spacing) {
  content::WebContents* web_contents = web_ui_->GetWebContents();
  SkColor foreground_skcolor =
      web_contents->GetColorProvider().GetColor(foreground_color_id);
  SkColor background_skcolor =
      web_contents->GetColorProvider().GetColor(background_color_id);

  page_->OnThemeChanged(
      ReadAnythingTheme::New(font_name, font_scale, foreground_skcolor,
                             background_skcolor, line_spacing, letter_spacing));
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void ReadAnythingPageHandler::ScreenAIServiceReady() {
  page_->ScreenAIServiceReady();
}
#endif

void ReadAnythingPageHandler::OnLinkClicked(const ui::AXTreeID& target_tree_id,
                                            ui::AXNodeID target_node_id) {
  if (delegate_) {
    delegate_->OnLinkClicked(target_tree_id, target_node_id);
  }
}

void ReadAnythingPageHandler::OnSelectionChange(
    const ui::AXTreeID& target_tree_id,
    ui::AXNodeID anchor_node_id,
    int anchor_offset,
    ui::AXNodeID focus_node_id,
    int focus_offset) {
  if (delegate_) {
    delegate_->OnSelectionChange(target_tree_id, anchor_node_id, anchor_offset,
                                 focus_node_id, focus_offset);
  }
}
