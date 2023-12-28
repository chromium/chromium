// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_HOME_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_HOME_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view_tracker.h"

class PrefService;

class HomePageUndoBubbleCoordinator {
 public:
  HomePageUndoBubbleCoordinator(views::View* anchor_view, PrefService* prefs);
  HomePageUndoBubbleCoordinator(const HomePageUndoBubbleCoordinator&) = delete;
  HomePageUndoBubbleCoordinator& operator=(
      const HomePageUndoBubbleCoordinator&) = delete;
  ~HomePageUndoBubbleCoordinator();

  void Show(const GURL& undo_url, bool undo_value_is_ntp);

 private:
  const raw_ptr<views::View> anchor_view_;
  const raw_ptr<PrefService> prefs_;
  views::ViewTracker tracker_;
};

class HomeButton : public ToolbarButton {
  METADATA_HEADER(HomeButton, ToolbarButton)

 public:
  explicit HomeButton(PressedCallback callback = PressedCallback(),
                      PrefService* prefs = nullptr);
  HomeButton(const HomeButton&) = delete;
  HomeButton& operator=(const HomeButton&) = delete;
  ~HomeButton() override;

  // ToolbarButton:
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool CanDrop(const OSExchangeData& data) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  views::View::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;

 private:
  void UpdateHomePage(
      const ui::DropTargetEvent& event,
      ui::mojom::DragOperation& output_drag_op,
      std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  const raw_ptr<PrefService> prefs_;
  HomePageUndoBubbleCoordinator coordinator_;

  base::WeakPtrFactory<HomeButton> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */, HomeButton, ToolbarButton)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, HomeButton)

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_HOME_BUTTON_H_
