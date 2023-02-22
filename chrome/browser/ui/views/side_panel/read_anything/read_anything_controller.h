// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_font_combobox.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toolbar_view.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_page_handler.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "ui/base/models/combobox_model.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "components/services/screen_ai/public/cpp/screen_ai_install_state.h"
#endif

class Browser;

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingController
//
//  A class that controls the Read Anything feature. This class does all of the
//  business logic of this feature and updates the model.
//  The controller is meant to be internal to the Read Anything feature and
//  classes outside this feature should not be making calls to it. The
//  coordinator is the external-facing API.
//  This class is owned by the ReadAnythingCoordinator and has the same lifetime
//  as the browser.
//
class ReadAnythingController : public ReadAnythingToolbarView::Delegate,
                               public ReadAnythingFontCombobox::Delegate,
                               public ReadAnythingPageHandler::Delegate,
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
                               public screen_ai::ScreenAIInstallState::Observer,
#endif
                               public TabStripModelObserver {
 public:
  ReadAnythingController(ReadAnythingModel* model, Browser* browser);
  ReadAnythingController(const ReadAnythingController&) = delete;
  ReadAnythingController& operator=(const ReadAnythingController&) = delete;
  ~ReadAnythingController() override;

  // Called to activate or de-activate Read Anything. The feature is active when
  // it is currently shown in the side panel.
  void Activate(bool active);
  bool IsActiveForTesting() { return active_; }

  // Called by ReadAnythingWebContentsObserver. Sends |details| to the WebUI.
  void AccessibilityEventReceived(
      const content::AXEventNotificationDetails& details);

  // Called by ReadAnythingWebContentsObserver. Notifies the WebUI that the
  // AXTree for |web_contents| has been destroyed.
  void WebContentsDestroyed(content::WebContents* web_contents);

 private:
  friend class ReadAnythingControllerTest;

  // ReadAnythingFontCombobox::Delegate:
  void OnFontChoiceChanged(int new_index) override;
  ui::ComboboxModel* GetFontComboboxModel() override;

  // ReadAnythingToolbarView::Delegate:
  void OnFontSizeChanged(bool increase) override;
  void OnColorsChanged(int new_index) override;
  ReadAnythingMenuModel* GetColorsModel() override;
  void OnLineSpacingChanged(int new_index) override;
  ReadAnythingMenuModel* GetLineSpacingModel() override;
  void OnLetterSpacingChanged(int new_index) override;
  ReadAnythingMenuModel* GetLetterSpacingModel() override;

  // ReadAnythingPageHandler::Delegate:
  void OnUIReady() override;
  void OnUIDestroyed() override;
  void OnLinkClicked(const ui::AXTreeID& target_tree_id,
                     const ui::AXNodeID& target_node_id) override;
  void OnSelectionChange(const ui::AXTreeID& target_tree_id,
                         const ui::AXNodeID& anchor_node_id,
                         int anchor_offset,
                         const ui::AXNodeID& focus_node_id,
                         int focus_offset) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabStripModelDestroyed(TabStripModel* tab_strip_model) override;

  // Notifies the model that the AXTreeID has changed.
  void NotifyActiveAXTreeIDChanged();

  // Create a web contents observer for the active tab and enable web
  // contents-only accessibility. This causes AXTreeSerializer to reset and send
  // accessibility events of the AXTree when it is re-serialized. The WebUI
  // receives these events and stores a copy of each web contents' AXTree. If
  // the UI was destroyed, it stops receiving events. OnUIReady is called when
  // it is re-created, indicating that it needs to restore its copy of each
  // web contents' AXTree.
  void ObserveAccessibilityEventsOnActiveTab();

  const raw_ptr<ReadAnythingModel> model_;

  // ReadAnythingController is owned by ReadAnythingCoordinator which is a
  // browser user data, so this pointer is always valid.
  raw_ptr<Browser> browser_;

  // Whether the Read Anything feature is currently active. The feature is
  // active when it is currently shown in the Side Panel.
  bool active_ = false;

  // Whether the Read Anything feature's UI is ready. This is set to true when
  // the UI is constructed and false when it is destroyed.
  bool ui_ready_ = false;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // screen_ai::ScreenAIInstallState::Observer:
  void StateChanged(screen_ai::ScreenAIInstallState::State state) override;

  // Observes the install state of ScreenAI. When ScreenAI is ready, notifies
  // the WebUI.
  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          screen_ai::ScreenAIInstallState::Observer>
      component_ready_observer_{this};
#endif

  base::WeakPtrFactory<ReadAnythingController> weak_pointer_factory_{this};
};
#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
