// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_HOVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_HOVER_BUTTON_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/controls/hover_button.h"

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
  // Creates a hoverable button that displays the string given by
  // |title_text| and |secondary_text| and displays the latter part in the
  // secondary text color. Optional |action_image_icon| is shown on right side.
  // |secondary_text| is shown on right side before the |action_image_icon|.
  // |tooltip_text| is used for the tooltip shown on hovering over the button.
  // *-------------------------------------------------------------------------*
  // | Icon | |title_text|          |secondary_text| State image | Action icon |
  // |-------------------------------------------------------------------------|
  // |      | |subtitle_text|                                                  |
  // *-------------------------------------------------------------------------*
  RichHoverButton(
      views::Button::PressedCallback callback,
      const ui::ImageModel& main_image_icon,
      const std::u16string& title_text,
      const std::u16string& secondary_text,
      const std::u16string& tooltip_text,
      const std::u16string& subtitle_text,
      std::optional<ui::ImageModel> action_image_icon = std::nullopt,
      std::optional<ui::ImageModel> state_icon = std::nullopt);

  RichHoverButton(const RichHoverButton&) = delete;
  RichHoverButton& operator=(const RichHoverButton&) = delete;

  ~RichHoverButton() override = default;

  void SetTitleText(const std::u16string& title_text);

  void SetSecondaryText(const std::u16string& secondary_text);

  void SetSubtitleText(const std::u16string& subtitle_text);

  void SetSubtitleMultiline(bool is_multiline);

  views::Label* title() { return title_; }
  views::Label* secondary_label() { return secondary_label_; }
  views::Label* subtitle() { return subtitle_; }

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

  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> secondary_label_ = nullptr;
  raw_ptr<views::Label> subtitle_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTROLS_RICH_HOVER_BUTTON_H_
