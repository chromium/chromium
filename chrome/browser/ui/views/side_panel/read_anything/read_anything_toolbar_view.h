// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOOLBAR_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_button_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_font_combobox.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/view.h"

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingToolbarView
//
//  The toolbar for Read Anything.
//  This class is created by the ReadAnythingCoordinator and owned by the
//  ReadAnythingContainerView. It has the same lifetime as the Side Panel view.
//
class ReadAnythingToolbarView : public views::View,
                                public ReadAnythingModel::Observer,
                                public ReadAnythingCoordinator::Observer {
 public:
  class Delegate {
   public:
    virtual void OnFontSizeChanged(bool increase) = 0;
    virtual void OnColorsChanged(int new_index) = 0;
    virtual ui::ComboboxModel* GetColorsModel() = 0;
    virtual void OnLetterSpacingChanged(int new_index) = 0;
    virtual ui::ComboboxModel* GetLetterSpacingModel() = 0;
  };

  ReadAnythingToolbarView(
      ReadAnythingCoordinator* coordinator,
      ReadAnythingToolbarView::Delegate* toolbar_delegate,
      ReadAnythingFontCombobox::Delegate* font_combobox_delegate);
  ReadAnythingToolbarView(const ReadAnythingToolbarView&) = delete;
  ReadAnythingToolbarView& operator=(const ReadAnythingToolbarView&) = delete;
  ~ReadAnythingToolbarView() override;

  // ReadAnythingModel::Observer:
  void OnReadAnythingThemeChanged(
      read_anything::mojom::ReadAnythingThemePtr new_theme) override;

  // ReadAnythingCoordinator::Observer:
  void OnCoordinatorDestroyed() override;

 private:
  friend class ReadAnythingToolbarViewTest;

  void DecreaseFontSizeCallback();
  void IncreaseFontSizeCallback();
  void ChangeColorsCallback();
  void ChangeLetterSpacingCallback();

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  std::unique_ptr<views::View> Separator();

  raw_ptr<views::Combobox> font_combobox_;
  raw_ptr<ReadAnythingButtonView> decrease_text_size_button_;
  raw_ptr<ReadAnythingButtonView> increase_text_size_button_;
  raw_ptr<views::Combobox> colors_combobox_;
  raw_ptr<views::Combobox> letter_spacing_combobox_;

  raw_ptr<ReadAnythingToolbarView::Delegate> delegate_;
  raw_ptr<ReadAnythingCoordinator> coordinator_;

  base::WeakPtrFactory<ReadAnythingToolbarView> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOOLBAR_VIEW_H_
