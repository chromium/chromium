// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HOVER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_HOVER_BUTTON_H_

#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button.h"

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

class PageInfoBubbleViewBrowserTest;

// A special class used for wrapping a single line styled label.
// |views::StyledLabel|s are all multi-line. With a layout manager,
// |StyledLabel| will try use the available space to size itself, and long
// titles will wrap to the next line (for smaller |HoverButton|s, this will
// also cover up |subtitle_|). Wrap it in a parent view with no layout manager
// to ensure it keeps its original size set by SizeToFit(). Long titles
// will then be truncated.
class SingleLineStyledLabelWrapper : public views::View {
 public:
  explicit SingleLineStyledLabelWrapper(const base::string16& title);
  ~SingleLineStyledLabelWrapper() override = default;

  // views::View
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  views::StyledLabel* label();

 private:
  views::StyledLabel* label_;
};

// A button taking the full width of its parent that shows a background color
// when hovered over.
class HoverButton : public views::LabelButton {
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
  // will vertically center |title|. |secondary_view|, when set, is shown
  // on the opposite side of the button from |icon_view|.
  // When |resize_row_for_secondary_icon| is false, the button tries to
  // accommodate the view's preferred size by reducing the top and bottom
  // insets appropriately up to a value of 0.
  HoverButton(views::ButtonListener* button_listener,
              std::unique_ptr<views::View> icon_view,
              const base::string16& title,
              const base::string16& subtitle,
              std::unique_ptr<views::View> secondary_view = nullptr,
              bool resize_row_for_secondary_view = true,
              bool secondary_view_can_process_events = false);

  ~HoverButton() override;

  static SkColor GetInkDropColor(const views::View* view);

  // views::LabelButton:
  void SetBorder(std::unique_ptr<views::Border> b) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Insets GetInsets() const override;

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
  // views::MenuButton:
  KeyClickAction GetKeyClickActionForEvent(const ui::KeyEvent& event) override;
  void StateChanged(ButtonState old_state) override;
  SkColor GetInkDropBaseColor() const override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  views::StyledLabel* title() const { return title_; }
  views::Label* subtitle() const { return subtitle_; }
  views::View* icon_view() const { return icon_view_; }
  views::View* secondary_view() const { return secondary_view_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(media_router::CastDialogSinkButtonTest,
                           SetTitleLabel);
  FRIEND_TEST_ALL_PREFIXES(media_router::CastDialogSinkButtonTest,
                           SetStatusLabel);
  FRIEND_TEST_ALL_PREFIXES(ExtensionsMenuItemViewTest,
                           NotifyClickExecutesAction);
  FRIEND_TEST_ALL_PREFIXES(ExtensionsMenuItemViewTest,
                           UpdatesToDisplayCorrectActionTitle);
  friend class PageInfoBubbleViewBrowserTest;

  views::StyledLabel* title_ = nullptr;
  views::Label* subtitle_ = nullptr;
  views::View* icon_view_ = nullptr;
  views::View* secondary_view_ = nullptr;

  // The horizontal space the padding and icon take up. Used for calculating the
  // available space for |title_|, if it exists.
  int taken_width_ = 0;

  // Custom insets, when secondary_view_ is larger than the rest of the row.
  base::Optional<gfx::Insets> insets_;

  // Whether this |HoverButton|'s accessible name and tooltip should be computed
  // from the |title_| and |subtitle_| text.
  bool auto_compute_tooltip_ = true;

  // Listener to be called when button is clicked.
  views::ButtonListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(HoverButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HOVER_BUTTON_H_
