// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOOLBAR_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_button_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_font_combobox.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_menu_button.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_menu_model.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toggle_button_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingToolbarView
//
//  The toolbar for Read Anything.
//  This class is either created by the ReadAnythingCoordinator (when the side
//  panel entry is global) or the ReadAnythingSidePanelController (when the side
//  panel entry is local) and owned by the ReadAnythingContainerView. It has the
//  same lifetime as the Side Panel view.
//
class ReadAnythingToolbarView
    : public views::View,
      public ReadAnythingModel::Observer,
      public ReadAnythingCoordinator::Observer,
      public ReadAnythingSidePanelController::Observer {
  METADATA_HEADER(ReadAnythingToolbarView, views::View)

 public:
  class Delegate {
   public:
    virtual void OnFontSizeChanged(bool increase) = 0;
    virtual void OnColorsChanged(int new_index) = 0;
    virtual ReadAnythingMenuModel* GetColorsModel() = 0;
    virtual void OnLineSpacingChanged(int new_index) = 0;
    virtual ReadAnythingMenuModel* GetLineSpacingModel() = 0;
    virtual void OnLetterSpacingChanged(int new_index) = 0;
    virtual ReadAnythingMenuModel* GetLetterSpacingModel() = 0;
    virtual void OnSystemThemeChanged() = 0;
    virtual void OnLinksEnabledChanged(bool is_enabled) = 0;
    virtual bool GetLinksEnabled() = 0;
  };

  ReadAnythingToolbarView(
      ReadAnythingCoordinator* coordinator,
      ReadAnythingToolbarView::Delegate* toolbar_delegate,
      ReadAnythingFontCombobox::Delegate* font_combobox_delegate);
  ReadAnythingToolbarView(
      ReadAnythingSidePanelController* controller,
      ReadAnythingToolbarView::Delegate* toolbar_delegate,
      ReadAnythingFontCombobox::Delegate* font_combobox_delegate);
  ReadAnythingToolbarView(const ReadAnythingToolbarView&) = delete;
  ReadAnythingToolbarView& operator=(const ReadAnythingToolbarView&) = delete;
  ~ReadAnythingToolbarView() override;

  // ReadAnythingModel::Observer:
  void OnReadAnythingThemeChanged(
      const std::string& font_name,
      double font_scale,
      bool links_enabled,
      ui::ColorId foreground_color_id,
      ui::ColorId background_color_id,
      ui::ColorId separator_color_id,
      ui::ColorId dropdown_color_id,
      ui::ColorId selected_dropdown_color_id,
      ui::ColorId focus_ring_color_id,
      read_anything::mojom::LineSpacing line_spacing,
      read_anything::mojom::LetterSpacing letter_spacing) override;

  // ReadAnythingCoordinator::Observer:
  void OnCoordinatorDestroyed() override;
  // ReadAnythingSidePanelController::Observer:
  void OnSidePanelControllerDestroyed() override;

 private:
  friend class ReadAnythingToolbarViewTest;

  void Init(ReadAnythingToolbarView::Delegate* toolbar_delegate,
            ReadAnythingFontCombobox::Delegate* font_combobox_delegate);
  void DecreaseFontSizeCallback();
  void IncreaseFontSizeCallback();
  void ChangeColorsCallback();
  void ChangeLineSpacingCallback();
  void ChangeLetterSpacingCallback();
  void LinksToggledCallback();
  void CleanUp();

  // views::View:
  void AddedToWidget() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  // Called when the system theme changes.
  void OnThemeChanged() override;

  std::unique_ptr<views::View> Separator();

  raw_ptr<ReadAnythingFontCombobox> font_combobox_;
  raw_ptr<ReadAnythingButtonView> decrease_text_size_button_;
  raw_ptr<ReadAnythingButtonView> increase_text_size_button_;
  raw_ptr<ReadAnythingToggleButtonView> toggle_links_button_;
  raw_ptr<ReadAnythingMenuButton> colors_button_;
  raw_ptr<ReadAnythingMenuButton> line_spacing_button_;
  raw_ptr<ReadAnythingMenuButton> letter_spacing_button_;
  std::vector<raw_ptr<views::Separator>> separators_;

  raw_ptr<ReadAnythingToolbarView::Delegate> delegate_;
  raw_ptr<ReadAnythingCoordinator> coordinator_;
  raw_ptr<ReadAnythingSidePanelController> controller_;

  base::WeakPtrFactory<ReadAnythingToolbarView> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOOLBAR_VIEW_H_
