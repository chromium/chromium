// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTROLS_TEXT_WITH_CONTROLS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CONTROLS_TEXT_WITH_CONTROLS_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace views {
class Label;
}  // namespace views

// A view that contains the layout for a container with controls (no leading
// icon).
// *-------------------------------------------------------------------------*
// |  Title                                      | Controls (buttons, icons) |
// |-------------------------------------------------------------------------|
// |  Secondary label(s)                         |                           |
// *-------------------------------------------------------------------------*
class TextWithControlsView : public views::FlexLayoutView {
  METADATA_HEADER(TextWithControlsView, views::FlexLayoutView)

 public:
  TextWithControlsView();

  void SetTitle(std::u16string title);
  void SetDescription(std::u16string description);
  void SetVisible(bool visible) override;

  views::Label* AddSecondaryLabel(std::u16string text);
  template <typename T>
  T* AddControl(std::unique_ptr<T> control_view) {
    control_view->SetProperty(views::kInternalPaddingKey,
                              control_view->GetInsets());
    controls_width_ += control_view->GetPreferredSize().width();
    return AddChildView(std::move(control_view));
  }

  int GetFirstLineHeight();
  gfx::Size FlexRule(const views::View* view,
                     const views::SizeBounds& maximum_size) const;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  views::Label* title() { return title_; }
  views::Label* description() { return description_; }

 private:
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> description_ = nullptr;
  raw_ptr<views::View> labels_wrapper_ = nullptr;

  // The sum of width of all control views in the right side of the row.
  int controls_width_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTROLS_TEXT_WITH_CONTROLS_VIEW_H_
