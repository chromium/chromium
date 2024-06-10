// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTAINER_VIEW_H_

#include <memory>

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"

class ReadAnythingToolbarView;
class ReadAnythingSidePanelWebView;

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingContainerView
//
//  A class that holds all of the Read Anything UI. This includes a toolbar,
//  which is a View, and the Read Anything contents pane, which is a WebUI.
//  This class is either created by the ReadAnythingCoordinator (when the side
//  panel is global) or the ReadAnythingSidePanelController (when the side panel
//  is local) and owned by the Side Panel View. It has the same lifetime as the
//  Side Panel view.
//
class ReadAnythingContainerView
    : public views::View,
      public ReadAnythingModel::Observer,
      public ReadAnythingCoordinator::Observer,
      public ReadAnythingSidePanelController::Observer {
  METADATA_HEADER(ReadAnythingContainerView, views::View)

 public:
  ReadAnythingContainerView(
      ReadAnythingCoordinator* coordinator,
      std::unique_ptr<ReadAnythingToolbarView> toolbar,
      std::unique_ptr<ReadAnythingSidePanelWebView> content);
  ReadAnythingContainerView(
      ReadAnythingSidePanelController* controller,
      std::unique_ptr<ReadAnythingToolbarView> toolbar,
      std::unique_ptr<ReadAnythingSidePanelWebView> content);
  ReadAnythingContainerView(const ReadAnythingContainerView&) = delete;
  ReadAnythingContainerView& operator=(const ReadAnythingContainerView&) =
      delete;
  ~ReadAnythingContainerView() override;

  // ReadAnythingModel::Observer:
  void OnReadAnythingThemeChanged(
      const std::string& font_name,
      double font_scale,
      bool links_enabled,
      bool images_enabled,
      ui::ColorId foreground_color_id,
      ui::ColorId background_color_id,
      ui::ColorId separator_color_id,
      ui::ColorId dropdown_color_id,
      ui::ColorId selection_color_id,
      ui::ColorId focus_ring_color_id,
      read_anything::mojom::LineSpacing line_spacing,
      read_anything::mojom::LetterSpacing letter_spacing) override;

  // ReadAnythingCoordinator::Observer:
  void OnCoordinatorDestroyed() override;
  // ReadAnythingSidePanelController::Observer:
  void OnSidePanelControllerDestroyed() override;

 private:
  void Init(std::unique_ptr<ReadAnythingToolbarView> toolbar,
            std::unique_ptr<ReadAnythingSidePanelWebView> content);

  raw_ptr<ReadAnythingCoordinator> coordinator_;
  raw_ptr<ReadAnythingSidePanelController> controller_;
  raw_ptr<views::Separator> separator_;
};
#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTAINER_VIEW_H_
