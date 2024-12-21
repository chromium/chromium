// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_HOVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_HOVER_BUTTON_H_

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/extend.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "ui/views/layout/table_layout.h"

namespace test {
class PageInfoBubbleViewTestApi;
}  // namespace test

namespace ui {
class ImageModel;
}  // namespace ui

namespace views {
class Label;
class View;
}  // namespace views

// Hoverable button containing various components:
// *--------------------------------------------------------*
// | Icon | Title               | State image | Action icon |
// |--------------------------------------------------------|
// |      | Custom view         |             |             |
// |--------------------------------------------------------|
// |      | Subtitle            |             |             |
// |      | (multiline)         |             |             |
// *--------------------------------------------------------*
//
// 'RichHoverButton' inherits the interaction behavior from 'HoverButton'
// but sets up its own layout and content.
// TODO(pkasting): This class should subclass Button, not HoverButton.
class RichHoverButton : public HoverButton {
  METADATA_HEADER(RichHoverButton, HoverButton)

 public:
  RichHoverButton(
      views::Button::PressedCallback callback,
      const ui::ImageModel& main_image_icon,
      const std::u16string& title_text,
      const std::u16string& subtitle_text,
      std::optional<ui::ImageModel> action_image_icon = std::nullopt,
      std::optional<ui::ImageModel> state_icon = std::nullopt);

  RichHoverButton(const RichHoverButton&) = delete;
  RichHoverButton& operator=(const RichHoverButton&) = delete;

  ~RichHoverButton() override;

  void SetTitleText(const std::u16string& title_text);

  void SetSubtitleText(const std::u16string& subtitle_text);

  void SetSubtitleMultiline(bool is_multiline);

  // TODO(crbug.com/40281048): Remove; at least color, and possibly both of
  // these, should instead be computed automatically from a single context value
  // on the button.
  void SetTitleTextStyleAndColor(int style, ui::ColorId);
  void SetSubtitleTextStyleAndColor(int style, ui::ColorId);

  // Sets the custom view. Pass an empty `std::unique_ptr<views::View>` to
  // reset.
  template <typename T>
  T* SetCustomView(std::unique_ptr<T> custom_view) {
    T* view = nullptr;
    if (!custom_view) {
      for (const auto& v : custom_view_row_views_) {
        RemoveChildViewT(v);
      }
      custom_view_row_views_.clear();
    } else if (custom_view_row_views_.empty()) {
      size_t start = custom_view_row_start_;
      custom_view_row_views_.push_back(AddChildViewAt(
          std::make_unique<views::View>(), start++));  // Skip main icon column.
      view = AddChildViewAt(std::move(custom_view), start++);
      custom_view_row_views_.push_back(view);
      base::Extend(custom_view_row_views_, AddFillerViews(start));
    } else {
      CHECK_GT(custom_view_row_views_.size(), 1u);
      const size_t index = *GetIndexOf(custom_view_row_views_[1]);
      RemoveChildViewT(custom_view_row_views_[1]);
      view = AddChildViewAt(std::move(custom_view), index);
      custom_view_row_views_[1] = view;
    }

    RecreateLayout();
    return view;
  }

  const views::Label* GetTitleViewForTesting() const;
  const views::Label* GetSubTitleViewForTesting() const;

 protected:
  // HoverButton:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  // Recreates the table layout, which must be done any time the custom view
  // changes between empty and non-empty (since its row is not filled with
  // placeholder `View`s when absent).
  // TODO(pkasting): This class should lay out using box, not table, with
  // top-aligned children, and add enough padding to the
  // icons/custom view/subtitle to properly align with the title. That would
  // obviate the need to recreate the layout after construction.
  void RecreateLayout();

  void UpdateAccessibleName();

  // Add filler views for state icon (if set) and action icon columns. Used for
  // the table rows after the first one.
  std::vector<raw_ptr<views::View>> AddFillerViews(size_t start);

  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::ImageView> state_icon_ = nullptr;
  size_t custom_view_row_start_;
  std::vector<raw_ptr<views::View>> custom_view_row_views_;
  std::vector<raw_ptr<views::View>> subtitle_row_views_;
  raw_ptr<views::Label> subtitle_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_HOVER_BUTTON_H_
