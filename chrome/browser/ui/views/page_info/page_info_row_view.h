// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_ROW_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/models/image_model.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {
class ImageView;
class FlexLayout;
class Label;
}  // namespace views

namespace test {
class PageInfoBubbleViewTestApi;
}  // namespace test

// A view that contains basic layout for rows in page info.
// *-------------------------------------------------------------------------*
// | Icon | Title                                | Controls (buttons, icons) |
// |-------------------------------------------------------------------------|
// |      | Secondary label                      |                           |
// *-------------------------------------------------------------------------*
class PageInfoRowView : public views::View {
 public:
  PageInfoRowView();

  void SetIcon(const ui::ImageModel image);
  void SetTitle(std::u16string title);
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

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

 private:
  friend class test::PageInfoBubbleViewTestApi;

  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::View> labels_wrapper_ = nullptr;

  // The sum of width of all control views in the right side of the row.
  int controls_width_ = 0;
  raw_ptr<views::FlexLayout> layout_manager_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_ROW_VIEW_H_
