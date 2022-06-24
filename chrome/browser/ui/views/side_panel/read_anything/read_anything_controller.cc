// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"

#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "ui/accessibility/ax_tree_update.h"

ReadAnythingController::ReadAnythingController(ReadAnythingModel* model,
                                               Browser* browser)
    : model_(model), browser_(browser) {
  DCHECK(browser_);
  if (browser_->tab_strip_model())
    browser_->tab_strip_model()->AddObserver(this);
}

ReadAnythingController::~ReadAnythingController() {
  DCHECK(browser_);
  if (browser_->tab_strip_model())
    browser_->tab_strip_model()->RemoveObserver(this);
  WebContentsObserver::Observe(nullptr);
}

void ReadAnythingController::Activate(bool active) {
  active_ = active;
  DistillAXTree();
}

void ReadAnythingController::OnFontChoiceChanged(int new_choice) {
  model_->SetSelectedFontIndex(new_choice);
}

void ReadAnythingController::OnUIReady() {
  DistillAXTree();
}

void ReadAnythingController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed())
    return;
  DistillAXTree();
}

void ReadAnythingController::DidStopLoading() {
  DistillAXTree();
}

void ReadAnythingController::DistillAXTree() {
  DCHECK(browser_);
  if (!active_)
    return;
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;
  WebContentsObserver::Observe(web_contents);

  // Read Anything just runs on the main frame and does not run on embedded
  // content.
  content::RenderFrameHost* render_frame_host =
      web_contents->GetPrimaryMainFrame();
  if (!render_frame_host)
    return;

  // Request a distilled AXTree for the main frame.
  render_frame_host->RequestDistilledAXTree(
      base::BindOnce(&ReadAnythingController::OnAXTreeDistilled,
                     weak_pointer_factory_.GetWeakPtr()));
}

void ReadAnythingController::OnAXTreeDistilled(
    const ui::AXTreeUpdate& snapshot,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  model_->SetDistilledAXTree(snapshot, content_node_ids);
}
