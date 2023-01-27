// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_constants.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/accessibility/ax_action_data.h"

class ReadAnythingWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ReadAnythingWebContentsObserver> {
 public:
  ReadAnythingWebContentsObserver(const ReadAnythingWebContentsObserver&) =
      delete;
  ReadAnythingWebContentsObserver& operator=(
      const ReadAnythingWebContentsObserver&) = delete;
  ~ReadAnythingWebContentsObserver() override = default;

  // content::WebContentsObserver:
  void AccessibilityEventReceived(
      const content::AXEventNotificationDetails& details) override {
    if (controller_) {
      controller_->AccessibilityEventReceived(details);
    }
  }

  void WebContentsDestroyed() override {
    if (controller_) {
      controller_->WebContentsDestroyed(web_contents());
    }
  }

  // This causes AXTreeSerializer to reset and send accessibility events of the
  // AXTree when it is re-serialized.
  void EnableAccessibility() {
    // TODO(crbug.com/1266555): Only enable kReadAnythingAXMode.
    web_contents()->EnableWebContentsOnlyAccessibilityMode();
  }

  void SetController(ReadAnythingController* controller) {
    controller_ = controller;
  }

 private:
  friend class content::WebContentsUserData<ReadAnythingWebContentsObserver>;

  explicit ReadAnythingWebContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        content::WebContentsUserData<ReadAnythingWebContentsObserver>(
            *web_contents) {}

  raw_ptr<ReadAnythingController> controller_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(ReadAnythingWebContentsObserver);

ReadAnythingController::ReadAnythingController(ReadAnythingModel* model,
                                               Browser* browser)
    : model_(model), browser_(browser) {
  DCHECK(browser_);
  browser_->tab_strip_model()->AddObserver(this);
}

ReadAnythingController::~ReadAnythingController() {
  TabStripModelObserver::StopObservingAll(this);
  for (auto* web_contents : AllTabContentses()) {
    ReadAnythingWebContentsObserver* observer =
        ReadAnythingWebContentsObserver::FromWebContents(web_contents);
    if (observer) {
      observer->SetController(nullptr);
    }
  }
}

void ReadAnythingController::Activate(bool active) {
  active_ = active;
  NotifyActiveAXTreeIDChanged();
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
  NotifyActiveAXTreeIDChanged();
}

void ReadAnythingController::OnUIDestroyed() {
  ui_ready_ = false;
}

void ReadAnythingController::OnLinkClicked(const ui::AXTreeID& target_tree_id,
                                           const ui::AXNodeID& target_node_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromAXTreeID(target_tree_id);
  if (!render_frame_host) {
    return;
  }
  ui::AXActionData action_data;
  action_data.target_tree_id = target_tree_id;
  action_data.action = ax::mojom::Action::kDoDefault;
  action_data.target_node_id = target_node_id;
  render_frame_host->AccessibilityPerformAction(action_data);
}

void ReadAnythingController::OnSelectionChange(
    const ui::AXTreeID& target_tree_id,
    const ui::AXNodeID& anchor_node_id,
    int anchor_offset,
    const ui::AXNodeID& focus_node_id,
    int focus_offset) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromAXTreeID(target_tree_id);
  if (!render_frame_host) {
    return;
  }
  ui::AXActionData action_data;
  action_data.target_tree_id = target_tree_id;
  action_data.action = ax::mojom::Action::kSetSelection;
  action_data.anchor_node_id = anchor_node_id;
  action_data.anchor_offset = anchor_offset;
  action_data.focus_node_id = focus_node_id;
  action_data.focus_offset = focus_offset;
  render_frame_host->AccessibilityPerformAction(action_data);
}

///////////////////////////////////////////////////////////////////////////////
// TabStripModelObserver:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!ui_ready_) {
    return;
  }
  if (selection.active_tab_changed()) {
    NotifyActiveAXTreeIDChanged();
  }
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

void ReadAnythingController::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& details) {
  model_->AccessibilityEventReceived(details);
}

void ReadAnythingController::WebContentsDestroyed(
    content::WebContents* web_contents) {
  content::RenderFrameHost* render_frame_host =
      web_contents->GetPrimaryMainFrame();
  if (!render_frame_host)
    return;
  ui::AXTreeID tree_id = render_frame_host->GetAXTreeID();
  model_->OnAXTreeDestroyed(tree_id);
}

void ReadAnythingController::NotifyActiveAXTreeIDChanged() {
  ui::AXTreeID tree_id = ui::AXTreeIDUnknown();
  if (active_) {
    content::WebContents* web_contents =
        browser_->tab_strip_model()->GetActiveWebContents();
    if (!web_contents) {
      return;
    }
    content::RenderFrameHost* render_frame_host =
        web_contents->GetPrimaryMainFrame();
    if (!render_frame_host) {
      return;
    }
    tree_id = render_frame_host->GetAXTreeID();
    ObserveAccessibilityEventsOnActiveTab();
  }
  model_->OnActiveAXTreeIDChanged(tree_id);
}

void ReadAnythingController::ObserveAccessibilityEventsOnActiveTab() {
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return;
  }
  // CreateForWebContents is no-op if an observer already exists.
  ReadAnythingWebContentsObserver::CreateForWebContents(web_contents);
  ReadAnythingWebContentsObserver* observer =
      ReadAnythingWebContentsObserver::FromWebContents(web_contents);
  observer->SetController(this);
  observer->EnableAccessibility();

  // TODO(crbug.com/1266555): Disable accessibility.and stop observing events
  // on the now inactive tab. But make sure that we don't disable it for
  // assistive technology users. Some options here are:
  // 1. Cache the current AXMode of the active web contents before enabling
  //    accessibility, and reset the mode to that mode when the tab becomes
  //    inactive.
  // 2. Set an AXContext on the web contents with web contents only mode
  //    enabled.
}
