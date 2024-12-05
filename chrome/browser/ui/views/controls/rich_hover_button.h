// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_HOVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_HOVER_BUTTON_H_

#include <string>

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

// Hoverable button containing icon, styled title, and (multi-line) subtitle.
// 'RichHoverButton' inherits the interaction behavior from 'HoverButton'
// but sets up its own layout and content.
class RichHoverButton : public HoverButton {
  METADATA_HEADER(RichHoverButton, HoverButton)

 public:
  // Creates a hoverable button that has an icon on the left side, followed by
  // |title_text| label. Optional |action_image_icon| and |state_icon| are shown
  // on right side. |subtile_text| is positioned directly under the
  // |title_text|.
  // *-------------------------------------------------------------------------*
  // | Icon | |title_text|                         | State image | Action icon |
  // |-------------------------------------------------------------------------|
  // |      | |subtitle_text|                                                  |
  // *-------------------------------------------------------------------------*
  RichHoverButton(
      views::Button::PressedCallback callback,
      const ui::ImageModel& main_image_icon,
      const std::u16string& title_text,
      const std::u16string& subtitle_text,
      std::optional<ui::ImageModel> action_image_icon = std::nullopt,
      std::optional<ui::ImageModel> state_icon = std::nullopt);

  RichHoverButton(const RichHoverButton&) = delete;
  RichHoverButton& operator=(const RichHoverButton&) = delete;

  ~RichHoverButton() override = default;

  void SetTitleText(const std::u16string& title_text);

  void SetSubtitleText(const std::u16string& subtitle_text);

  void SetSubtitleMultiline(bool is_multiline);

  // TODO(crbug.com/40281048): Remove; at least color, and possibly both of
  // these, should instead be computed automatically from a single context value
  // on the button.
  void SetTitleTextStyleAndColor(int style, ui::ColorId);
  void SetSubtitleTextStyleAndColor(int style, ui::ColorId);

  // Add custom view under the |title_text|.
  // ...
  // |-------------------------------------------------------------------------|
  // |      | |custom_view|                                                    |
  // *-------------------------------------------------------------------------*
  template <typename T>
  T* AddCustomSubtitle(std::unique_ptr<T> custom_view) {
    static_cast<views::TableLayout*>(GetLayoutManager())
        ->AddRows(1, views::TableLayout::kFixedSize);
    AddChildView(std::make_unique<views::View>());  // main icon column
    auto* view = AddChildView(std::move(custom_view));
    AddFillerViews();
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
  void UpdateAccessibleName();

  // Add filler views for state icon (if set) and action icon columns. Used for
  // the table rows after the first one.
  void AddFillerViews();

  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> subtitle_ = nullptr;

  bool has_state_icon_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_HOVER_BUTTON_H_
