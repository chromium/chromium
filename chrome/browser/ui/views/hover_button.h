// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HOVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_HOVER_BUTTON_H_

#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"

namespace gfx {
enum ElideBehavior;
class ImageSkia;
}  // namespace gfx

namespace media_router {
FORWARD_DECLARE_TEST(CastDialogSinkButtonTest, SetTitleLabel);
FORWARD_DECLARE_TEST(CastDialogSinkButtonTest, SetStatusLabel);
}  // namespace media_router

namespace views {
class ButtonListener;
class Label;
class StyledLabel;
class View;
}  // namespace views

// A button taking the full width of its parent that shows a background color
// when hovered over.
class HoverButton : public views::MenuButton, public views::MenuButtonListener {
 public:
  enum Style { STYLE_PROMINENT, STYLE_ERROR };

  // Creates a single line hover button with no icon.
  HoverButton(views::ButtonListener* button_listener,
              const base::string16& text);

  // Creates a single line hover button with an icon.
  HoverButton(views::ButtonListener* button_listener,
              const gfx::ImageSkia& icon,
              const base::string16& text);

  // Creates a HoverButton with custom subviews. |icon_view| replaces the
  // LabelButton icon, and titles appear on separate rows. An empty |subtitle|
  // will vertically center |title|. |secondary_icon_view|, when set, is shown
  // on the opposite side of the button from |icon_view|.
  HoverButton(views::ButtonListener* button_listener,
              std::unique_ptr<views::View> icon_view,
              const base::string16& title,
              const base::string16& subtitle,
              std::unique_ptr<views::View> secondary_icon_view = nullptr);

  ~HoverButton() override;

  // views::MenuButton:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void SetBorder(std::unique_ptr<views::Border> b) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool IsTriggerableEventType(const ui::Event& event) override;

  // Updates the title text, and applies the secondary style to the text
  // specified by |range|. If |range| is invalid, no style is applied. This
  // method is only supported for |HoverButton|s created with a title and
  // subtitle.
  void SetTitleTextWithHintRange(const base::string16& title_text,
                                 const gfx::Range& range);

  // This method is only supported for |HoverButton|s created with a title and
  // non-empty subtitle.
  void SetSubtitleElideBehavior(gfx::ElideBehavior elide_behavior);

  // Adjusts the background and the text color according to |style|.
  void SetStyle(Style style);

  // Sets the text style of the title considering the color of the background.
  // Passing |background_color| makes sure that the text color will not be
  // changed to a color that is not readable on the specified background.
  void SetTitleTextStyle(views::style::TextStyle text_style,
                         SkColor background_color);

  void SetSubtitleColor(SkColor color);

  void set_auto_compute_tooltip(bool auto_compute_tooltip) {
    auto_compute_tooltip_ = auto_compute_tooltip;
  }

 protected:
  // views::MenuButtonListener:
  void OnMenuButtonClicked(MenuButton* source,
                           const gfx::Point& point,
                           const ui::Event* event) override;

  // views::MenuButton:
  KeyClickAction GetKeyClickActionForEvent(const ui::KeyEvent& event) override;
  void StateChanged(ButtonState old_state) override;
  SkColor GetInkDropBaseColor() const override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  void Layout() override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  views::StyledLabel* title() const { return title_; }
  views::Label* subtitle() const { return subtitle_; }
  views::View* icon_view() const { return icon_view_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(media_router::CastDialogSinkButtonTest,
                           SetTitleLabel);
  FRIEND_TEST_ALL_PREFIXES(media_router::CastDialogSinkButtonTest,
                           SetStatusLabel);

  views::StyledLabel* title_;
  views::Label* subtitle_;
  views::View* icon_view_;
  views::View* secondary_icon_view_;

  // The horizontal space the padding and icon take up. Used for calculating the
  // available space for |title_|, if it exists.
  int taken_width_ = 0;

  // Whether this |HoverButton|'s accessible name and tooltip should be computed
  // from the |title_| and |subtitle_| text.
  bool auto_compute_tooltip_ = true;

  // Listener to be called when button is clicked.
  views::ButtonListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(HoverButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HOVER_BUTTON_H_
