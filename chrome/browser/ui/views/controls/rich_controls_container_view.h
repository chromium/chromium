// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_CONTROLS_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_CONTROLS_CONTAINER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace views {
class ImageView;
class Label;
class StyledLabel;
}  // namespace views

// A view that contains basic layout for a container with controls.
// *-------------------------------------------------------------------------*
// | Icon | Title                                | Controls (buttons, icons) |
// |-------------------------------------------------------------------------|
// |      | Secondary label(s)                   |                           |
// *-------------------------------------------------------------------------*
class RichControlsContainerView : public views::FlexLayoutView {
  METADATA_HEADER(RichControlsContainerView, views::FlexLayoutView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kIcon);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kEnforcedIcon);

  RichControlsContainerView();

  void SetIcon(const ui::ImageModel image);
  void SetEnforcedIcon(CookieControlsEnforcement enforcement);
  void SetTitle(std::u16string title);
  views::Label* AddSecondaryLabel(std::u16string text);
  views::StyledLabel* AddSecondaryStyledLabel(std::u16string text);
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
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  views::Label* title() { return title_; }

  const std::u16string& GetTitleForTesting();
  const ui::ImageModel GetIconForTesting();
  const ui::ImageModel GetEnforcedIconForTesting();

 private:
  virtual int GetMinBubbleWidth() const;

  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::ImageView> enforced_icon_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::View> labels_wrapper_ = nullptr;

  // The sum of width of all control views in the right side of the row.
  int controls_width_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_CONTROLS_CONTAINER_VIEW_H_
