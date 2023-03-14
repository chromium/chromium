// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingPageHandler
//
//  A handler of the Read Anything app
//  (chrome/browser/resources/side_panel/read_anything/app.ts).
//  This class is created and owned by ReadAnythingUI and has the same lifetime
//  as the Side Panel view.
//
class ReadAnythingPageHandler : public read_anything::mojom::PageHandler,
                                public ReadAnythingModel::Observer,
                                public ReadAnythingCoordinator::Observer {
 public:
  class Delegate {
   public:
    virtual void OnUIReady() = 0;
    virtual void OnUIDestroyed() = 0;
    virtual void OnLinkClicked(const ui::AXTreeID& target_tree_id,
                               const ui::AXNodeID& target_node_id) = 0;
    virtual void OnSelectionChange(const ui::AXTreeID& target_tree_id,
                                   const ui::AXNodeID& anchor_node_id,
                                   int anchor_offset,
                                   const ui::AXNodeID& focus_node_id,
                                   int focus_offset) = 0;
  };

  ReadAnythingPageHandler(
      mojo::PendingRemote<read_anything::mojom::Page> page,
      mojo::PendingReceiver<read_anything::mojom::PageHandler> receiver,
      content::WebUI* web_ui);
  ReadAnythingPageHandler(const ReadAnythingPageHandler&) = delete;
  ReadAnythingPageHandler& operator=(const ReadAnythingPageHandler&) = delete;
  ~ReadAnythingPageHandler() override;

  // read_anything::mojom::PageHandler:
  void OnLinkClicked(const ui::AXTreeID& target_tree_id,
                     ui::AXNodeID target_node_id) override;
  void OnSelectionChange(const ui::AXTreeID& target_tree_id,
                         ui::AXNodeID anchor_node_id,
                         int anchor_offset,
                         ui::AXNodeID focus_node_id,
                         int focus_offset) override;

  // ReadAnythingModel::Observer:
  void AccessibilityEventReceived(
      const content::AXEventNotificationDetails& details) override;
  void OnActiveAXTreeIDChanged(const ui::AXTreeID& tree_id,
                               const ukm::SourceId& ukm_source_id) override;
  void OnAXTreeDestroyed(const ui::AXTreeID& tree_id) override;
  void OnReadAnythingThemeChanged(
      const std::string& font_name,
      double font_scale,
      ui::ColorId foreground_color_id,
      ui::ColorId background_color_id,
      ui::ColorId separator_color_id,
      ui::ColorId dropdown_color_id,
      read_anything::mojom::LineSpacing line_spacing,
      read_anything::mojom::LetterSpacing letter_spacing) override;
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  void ScreenAIServiceReady() override;
#endif

  // ReadAnythingCoordinator::Observer:
  void OnCoordinatorDestroyed() override;

 private:
  raw_ptr<ReadAnythingCoordinator> coordinator_;
  raw_ptr<ReadAnythingPageHandler::Delegate> delegate_;

  const raw_ptr<Browser> browser_;

  const mojo::Receiver<read_anything::mojom::PageHandler> receiver_;
  const mojo::Remote<read_anything::mojom::Page> page_;

  const raw_ptr<content::WebUI> web_ui_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PAGE_HANDLER_H_
