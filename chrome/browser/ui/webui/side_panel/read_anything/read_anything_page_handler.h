// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "components/services/screen_ai/public/cpp/screen_ai_install_state.h"
#endif

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingPageHandler
//
//  A handler of the Read Anything app
//  (chrome/browser/resources/side_panel/read_anything/app.ts).
//  This class is created and owned by ReadAnythingUI and has the same lifetime
//  as the Side Panel view.
//
class ReadAnythingPageHandler
    : public ui::AXActionHandlerObserver,
      public read_anything::mojom::PageHandler,
      public ReadAnythingModel::Observer,
      public ReadAnythingCoordinator::Observer,
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
      public screen_ai::ScreenAIInstallState::Observer,
#endif
      public TabStripModelObserver,
      public content::WebContentsObserver {
 public:
  ReadAnythingPageHandler(
      mojo::PendingRemote<read_anything::mojom::Page> page,
      mojo::PendingReceiver<read_anything::mojom::PageHandler> receiver,
      content::WebUI* web_ui);
  ReadAnythingPageHandler(const ReadAnythingPageHandler&) = delete;
  ReadAnythingPageHandler& operator=(const ReadAnythingPageHandler&) = delete;
  ~ReadAnythingPageHandler() override;

 private:
  // ui::AXActionHandlerObserver:
  void TreeRemoved(ui::AXTreeID ax_tree_id) override;

  // read_anything::mojom::PageHandler:
  void OnLinkClicked(const ui::AXTreeID& target_tree_id,
                     ui::AXNodeID target_node_id) override;
  void OnSelectionChange(const ui::AXTreeID& target_tree_id,
                         ui::AXNodeID anchor_node_id,
                         int anchor_offset,
                         ui::AXNodeID focus_node_id,
                         int focus_offset) override;

  // ReadAnythingModel::Observer:
  void OnReadAnythingThemeChanged(
      const std::string& font_name,
      double font_scale,
      ui::ColorId foreground_color_id,
      ui::ColorId background_color_id,
      ui::ColorId separator_color_id,
      ui::ColorId dropdown_color_id,
      ui::ColorId selected_dropdown_color_id,
      read_anything::mojom::LineSpacing line_spacing,
      read_anything::mojom::LetterSpacing letter_spacing) override;

  // ReadAnythingCoordinator::Observer:
  void Activate(bool active) override;
  void OnCoordinatorDestroyed() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabStripModelDestroyed(TabStripModel* tab_strip_model) override;

  // content::WebContentsObserver:
  void AccessibilityEventReceived(
      const content::AXEventNotificationDetails& details) override;
  void PrimaryPageChanged(content::Page& page) override;

  // When the active web contents changes (or the UI becomes active):
  // 1. Begins observing the web contents of the active tab and enables web
  //    contents-only accessibility on that web contents. This causes
  //    AXTreeSerializer to reset and send accessibility events of the AXTree
  //    when it is re-serialized. The WebUI receives these events and stores a
  //    copy of the web contents' AXTree.
  // 2. Notifies the model that the AXTreeID has changed.
  void OnActiveWebContentsChanged();

  // Notifies the model that the AXTreeID has changed.
  void OnActiveAXTreeIDChanged();

  raw_ptr<ReadAnythingCoordinator> coordinator_;
  const raw_ptr<Browser> browser_;
  const raw_ptr<content::WebUI> web_ui_;

  const mojo::Receiver<read_anything::mojom::PageHandler> receiver_;
  const mojo::Remote<read_anything::mojom::Page> page_;

  // Whether the Read Anything feature is currently active. The feature is
  // active when it is currently shown in the Side Panel.
  bool active_ = true;

  // Observes the AXActionHandlerRegistry for AXTree removals.
  base::ScopedObservation<ui::AXActionHandlerRegistry,
                          ui::AXActionHandlerObserver>
      ax_action_handler_observer_{this};

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // screen_ai::ScreenAIInstallState::Observer:
  void StateChanged(screen_ai::ScreenAIInstallState::State state) override;

  // Observes the install state of ScreenAI. When ScreenAI is ready, notifies
  // the WebUI.
  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          screen_ai::ScreenAIInstallState::Observer>
      component_ready_observer_{this};
#endif
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_
