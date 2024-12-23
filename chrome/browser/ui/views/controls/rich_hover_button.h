// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_HOVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_HOVER_BUTTON_H_

#include <string_view>
#include <vector>

#include "base/containers/extend.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "ui/views/metadata/view_factory.h"

namespace test {
class PageInfoBubbleViewTestApi;
}  // namespace test

namespace ui {
class ImageModel;
}  // namespace ui

namespace views {
class ImageView;
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
  RichHoverButton();
  RichHoverButton(views::Button::PressedCallback callback,
                  ui::ImageModel icon,
                  std::u16string_view title_text,
                  std::u16string_view subtitle_text,
                  ui::ImageModel action_icon = ui::ImageModel(),
                  ui::ImageModel state_icon = ui::ImageModel());

  RichHoverButton(const RichHoverButton&) = delete;
  RichHoverButton& operator=(const RichHoverButton&) = delete;

  ~RichHoverButton() override;

  ui::ImageModel GetIcon() const;
  void SetIcon(ui::ImageModel icon);

  std::u16string_view GetTitleText() const;
  void SetTitleText(std::u16string_view title_text);

  ui::ImageModel GetStateIcon() const;
  void SetStateIcon(ui::ImageModel state_icon);

  ui::ImageModel GetActionIcon() const;
  void SetActionIcon(ui::ImageModel action_icon);

  std::u16string_view GetSubtitleText() const;
  void SetSubtitleText(std::u16string_view subtitle_text);

  bool GetSubtitleMultiline() const;
  void SetSubtitleMultiline(bool subtitle_multiline);

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
      RemoveChildViewT(custom_view_row_views_[1]);
      view = AddChildViewAt(std::move(custom_view), custom_view_row_start_ + 1);
      custom_view_row_views_[1] = view;
    }

    RecreateLayout();
    return view;
  }

 protected:
  // HoverButton:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  // Updates the image at `icon_member`, which corresponds to child
  // `child_index`, to be `icon`. If `icon` is empty, the member will be set to
  // null. If `use_placeholder` is true, empty `View`s are inserted (instead of
  // nothing) when the provided image is empty, to avoid changing the columns of
  // items in the table.
  void SetIconMember(raw_ptr<views::ImageView>& icon_member,
                     size_t child_index,
                     ui::ImageModel icon,
                     bool use_placeholder);

  // Recreates the table layout, which must be done any time the custom view or
  // subtitle change between empty and non-empty (since those rows are not
  // filled with placeholder `View`s when absent).
  // TODO(pkasting): This class should lay out using box, not table, with
  // top-aligned children, and add enough padding to the
  // icons/custom view/subtitle to properly align with the title. That would
  // obviate the need to recreate the layout after construction.
  void RecreateLayout();

  // Recomputes the accessible name, which is affected by both labels.
  void UpdateAccessibleName();

  // Adds filler views for state icon (if set) and action icon columns. Used for
  // the table rows after the first one.
  std::vector<raw_ptr<views::View>> AddFillerViews(size_t start);

  size_t start_;
  // `ImageView` pointers and `subtitle` are non-null only if present.
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> title_;  // Never null.
  raw_ptr<views::ImageView> state_icon_ = nullptr;
  raw_ptr<views::ImageView> action_icon_ = nullptr;
  size_t custom_view_row_start_;
  std::vector<raw_ptr<views::View>> custom_view_row_views_;
  std::vector<raw_ptr<views::View>> subtitle_row_views_;
  raw_ptr<views::Label> subtitle_ = nullptr;
  bool subtitle_multiline_ = true;
};

BEGIN_VIEW_BUILDER(, RichHoverButton, HoverButton)
VIEW_BUILDER_PROPERTY(ui::ImageModel, Icon)
VIEW_BUILDER_PROPERTY(std::u16string_view, TitleText)
VIEW_BUILDER_PROPERTY(ui::ImageModel, StateIcon)
VIEW_BUILDER_PROPERTY(ui::ImageModel, ActionIcon)
VIEW_BUILDER_PROPERTY(std::u16string_view, SubtitleText)
VIEW_BUILDER_PROPERTY(bool, SubtitleMultiline)
VIEW_BUILDER_METHOD(SetTitleTextStyleAndColor, int, ui::ColorId)
VIEW_BUILDER_METHOD(SetSubtitleTextStyleAndColor, int, ui::ColorId)
VIEW_BUILDER_TEMPLATED_PROPERTY(<typename T>, CustomView, std::unique_ptr<T>)

END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(, RichHoverButton)

#endif  // CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_HOVER_BUTTON_H_
