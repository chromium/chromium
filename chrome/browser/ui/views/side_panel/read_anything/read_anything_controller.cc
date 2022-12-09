// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"

#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "ui/accessibility/ax_tree_update.h"

ReadAnythingController::ReadAnythingController(ReadAnythingModel* model,
                                               Browser* browser)
    : model_(model), browser_(browser) {
  DCHECK(browser_);
  browser_->tab_strip_model()->AddObserver(this);
  WebContentsObserver::Observe(
      browser_->tab_strip_model()->GetActiveWebContents());
}

ReadAnythingController::~ReadAnythingController() {
  TabStripModelObserver::StopObservingAll(this);
  WebContentsObserver::Observe(nullptr);
}

void ReadAnythingController::Activate(bool active) {
  active_ = active;
  DistillAXTree();
}

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingFontCombobox::Delegate:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingController::OnFontChoiceChanged(int new_index) {
  if (!model_->GetFontModel()->IsValidFontIndex(new_index))
    return;

  model_->SetSelectedFontByIndex(new_index);

  browser_->profile()->GetPrefs()->SetString(
      prefs::kAccessibilityReadAnythingFontName,
      model_->GetFontModel()->GetFontNameAt(new_index));
}

ui::ComboboxModel* ReadAnythingController::GetFontComboboxModel() {
  return model_->GetFontModel();
}

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingToolbarView::Delegate:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingController::OnFontSizeChanged(bool increase) {
  if (increase) {
    model_->IncreaseTextSize();
  } else {
    model_->DecreaseTextSize();
  }

  browser_->profile()->GetPrefs()->SetDouble(
      prefs::kAccessibilityReadAnythingFontScale, model_->GetFontScale());
}

void ReadAnythingController::OnColorsChanged(int new_index) {
  if (!model_->GetColorsModel()->IsValidIndex(new_index))
    return;

  model_->SetSelectedColorsByIndex(new_index);

  browser_->profile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingColorInfo, new_index);
}

ReadAnythingMenuModel* ReadAnythingController::GetColorsModel() {
  return model_->GetColorsModel();
}

void ReadAnythingController::OnLineSpacingChanged(int new_index) {
  if (!model_->GetLineSpacingModel()->IsValidIndex(new_index))
    return;

  model_->SetSelectedLineSpacingByIndex(new_index);

  browser_->profile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingLineSpacing, new_index);
}

ReadAnythingMenuModel* ReadAnythingController::GetLineSpacingModel() {
  return model_->GetLineSpacingModel();
}

void ReadAnythingController::OnLetterSpacingChanged(int new_index) {
  if (!model_->GetLetterSpacingModel()->IsValidIndex(new_index))
    return;

  model_->SetSelectedLetterSpacingByIndex(new_index);

  browser_->profile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingLetterSpacing, new_index);
}

ReadAnythingMenuModel* ReadAnythingController::GetLetterSpacingModel() {
  return model_->GetLetterSpacingModel();
}

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingPageHandler::Delegate:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingController::OnUIReady() {
  ui_ready_ = true;
  DistillAXTree();
}

void ReadAnythingController::OnUIDestroyed() {
  ui_ready_ = false;
}

void ReadAnythingController::OnLinkClicked(const GURL& url,
                                           bool open_in_new_tab) {
  if (!web_contents())
    return;
  WindowOpenDisposition disposition =
      open_in_new_tab ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                      : WindowOpenDisposition::CURRENT_TAB;
  content::OpenURLParams params(url, content::Referrer(), disposition,
                                ui::PAGE_TRANSITION_LINK,
                                /* is_renderer_initiated= */ true);
  params.initiator_origin = url::Origin::Create(web_contents()->GetURL());
  browser_->OpenURL(params);
}

///////////////////////////////////////////////////////////////////////////////
// TabStripModelObserver:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed())
    return;
  WebContentsObserver::Observe(selection.new_contents);
  DistillAXTree();
}

void ReadAnythingController::OnTabStripModelDestroyed(
    TabStripModel* tab_strip_model) {
  // If the TabStripModel is destroyed before |this|, remove |this| as an
  // observer and set |browser_| to nullptr.
  DCHECK(browser_);
  tab_strip_model->RemoveObserver(this);
  browser_ = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// content::WebContentsObserver:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingController::DidStopLoading() {
  DistillAXTree();
}

///////////////////////////////////////////////////////////////////////////////

void ReadAnythingController::DistillAXTree() {
  DCHECK(browser_);
  if (!active_ || !ui_ready_ || !web_contents())
    return;

  // Read Anything just runs on the main frame and does not run on embedded
  // content.
  content::RenderFrameHost* render_frame_host =
      web_contents()->GetPrimaryMainFrame();
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
