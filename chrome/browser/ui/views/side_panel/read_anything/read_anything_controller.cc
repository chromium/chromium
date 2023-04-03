// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"

#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "components/services/screen_ai/public/cpp/screen_ai_service_router.h"
#include "components/services/screen_ai/public/cpp/screen_ai_service_router_factory.h"
#endif

ReadAnythingController::ReadAnythingController(ReadAnythingModel* model,
                                               Browser* browser)
    : model_(model), browser_(browser) {
  DCHECK(browser_);
  browser_->tab_strip_model()->AddObserver(this);
  ax_action_handler_observer_.Observe(
      ui::AXActionHandlerRegistry::GetInstance());
}

ReadAnythingController::~ReadAnythingController() {
  TabStripModelObserver::StopObservingAll(this);
  Observe(nullptr);
}

void ReadAnythingController::Activate(bool active) {
  active_ = active;
  OnActiveWebContentsChanged();
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

ReadAnythingFontModel* ReadAnythingController::GetFontComboboxModel() {
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

  // Saved preferences correspond to LineSpacing. However, since it contains a
  // deprecated value, the drop-down indices don't correspond exactly.
  LineSpacing line_spacing =
      model_->GetLineSpacingModel()->GetLineSpacingAt(new_index);
  browser_->profile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingLineSpacing,
      static_cast<size_t>(line_spacing));
}

ReadAnythingMenuModel* ReadAnythingController::GetLineSpacingModel() {
  return model_->GetLineSpacingModel();
}

void ReadAnythingController::OnLetterSpacingChanged(int new_index) {
  if (!model_->GetLetterSpacingModel()->IsValidIndex(new_index))
    return;

  model_->SetSelectedLetterSpacingByIndex(new_index);

  // Saved preferences correspond to LetterSpacing. However, since it contains a
  // deprecated value, the drop-down indices don't correspond exactly.
  LetterSpacing letter_spacing =
      model_->GetLetterSpacingModel()->GetLetterSpacingAt(new_index);
  browser_->profile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingLetterSpacing,
      static_cast<size_t>(letter_spacing));
}

ReadAnythingMenuModel* ReadAnythingController::GetLetterSpacingModel() {
  return model_->GetLetterSpacingModel();
}

void ReadAnythingController::OnSystemThemeChanged() {
  model_->OnSystemThemeChanged();
}

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingPageHandler::Delegate:
///////////////////////////////////////////////////////////////////////////////

void ReadAnythingController::OnUIReady() {
  ui_ready_ = true;
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (features::IsReadAnythingWithScreen2xEnabled()) {
    if (screen_ai::ScreenAIInstallState::GetInstance()
            ->IsComponentAvailable()) {
      // Notify that the screen ai service is already ready so we can bind to
      // the content extractor.
      model_->ScreenAIServiceReady();
    } else if (!component_ready_observer_.IsObserving()) {
      component_ready_observer_.Observe(
          screen_ai::ScreenAIInstallState::GetInstance());
    }
  }
#endif
  OnActiveWebContentsChanged();
}

void ReadAnythingController::OnUIDestroyed() {
  ui_ready_ = false;
  Observe(nullptr);
}

void ReadAnythingController::OnLinkClicked(const ui::AXTreeID& target_tree_id,
                                           const ui::AXNodeID& target_node_id) {
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

void ReadAnythingController::OnSelectionChange(
    const ui::AXTreeID& target_tree_id,
    const ui::AXNodeID& anchor_node_id,
    int anchor_offset,
    const ui::AXNodeID& focus_node_id,
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
    OnActiveWebContentsChanged();
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

void ReadAnythingController::TreeRemoved(ui::AXTreeID ax_tree_id) {
  model_->OnAXTreeDestroyed(ax_tree_id);
}

void ReadAnythingController::PrimaryPageChanged(content::Page& page) {
  OnActiveAXTreeIDChanged();
}

void ReadAnythingController::OnActiveWebContentsChanged() {
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

void ReadAnythingController::OnActiveAXTreeIDChanged() {
  ui::AXTreeID tree_id = ui::AXTreeIDUnknown();
  ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
  if (active_ && web_contents()) {
    content::RenderFrameHost* render_frame_host =
        web_contents()->GetPrimaryMainFrame();
    if (render_frame_host) {
      tree_id = render_frame_host->GetAXTreeID();
      ukm_source_id = render_frame_host->GetPageUkmSourceId();
    }
  }

  model_->OnActiveAXTreeIDChanged(tree_id, ukm_source_id);
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void ReadAnythingController::StateChanged(
    screen_ai::ScreenAIInstallState::State state) {
  DCHECK(features::IsReadAnythingWithScreen2xEnabled());
  // If Screen AI library is downloaded but not initialized yet, ensure it is
  // loadable and initializes without any problems.
  if (state == screen_ai::ScreenAIInstallState::State::kDownloaded) {
    screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(
        browser_->profile())
        ->LaunchIfNotRunning();
    return;
  }
  if (state == screen_ai::ScreenAIInstallState::State::kReady) {
    model_->ScreenAIServiceReady();
  }
}
#endif
