// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GLIC_GLIC_AND_ACTOR_BUTTONS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GLIC_GLIC_AND_ACTOR_BUTTONS_CONTAINER_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class InkDropContainerView;
class InkDropHost;
}  // namespace views

namespace glic {
class TabStripGlicButton;
}

// Container view for GlicButton and GlicActorTaskIcon. It can have a background
// and a highlight which change based on the state of the two buttons.
class GlicAndActorButtonsContainer : public views::FlexLayoutView {
  METADATA_HEADER(GlicAndActorButtonsContainer, views::FlexLayoutView)
 public:
  GlicAndActorButtonsContainer();
  ~GlicAndActorButtonsContainer() override;

  // Remove glic_button from its parent and insert it as this view's child.
  glic::TabStripGlicButton* InsertGlicButton(
      glic::TabStripGlicButton* glic_button);

  // Set or clear the ink drop highlight drawn over the background.
  void SetHighlighted(bool highlighted);

  // Set the base background color.
  void SetBackgroundColor(ui::ColorId color_id);

  // views::View:
  void AddLayerToRegion(ui::Layer* layer, views::LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* layer) override;
  void Layout(PassKey) override;

 private:
  views::InkDropHost* GetInkDropHost();
  void UpdateInkDrop();

  raw_ptr<views::InkDropContainerView> ink_drop_container_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GLIC_GLIC_AND_ACTOR_BUTTONS_CONTAINER_H_
