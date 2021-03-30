// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_TIP_MARQUEE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_TIP_MARQUEE_VIEW_H_

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/scoped_observation.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class StyledLabel;
}

// Displays a tip that can scroll out from a Chrome icon to a full sentence,
// optionally with a clickable tip.
//
//       🌏 Did you know that Chrome can display web pages? [ LEARN MORE ]
//                                       🌏 Did you know that Chrome ca...
//                                                                       🌏
class TipMarqueeView : public views::View, public views::WidgetObserver {
 public:
  METADATA_HEADER(TipMarqueeView);

  using LearnMoreLinkClickedCallback =
      base::RepeatingCallback<void(TipMarqueeView*)>;

  // Constructs a tip marquee view which will display text with the given
  // display parameters (see views::Label::Label() for usage).
  explicit TipMarqueeView(int text_context = views::style::CONTEXT_LABEL,
                          int text_style = views::style::STYLE_PRIMARY);
  ~TipMarqueeView() override;

  // Sets the tip and shows the view if there is adequate space. |tip_text| will
  // be displayed as plain text, and if |learn_more_link_clicked_callback| is
  // specified, a "learn more" link will be present that will call the callback
  // when clicked.
  //
  // Returns true if there is sufficient space in the parent view's layout to
  // display the fully expanded tip text and (if applicable) Learn More link.
  bool SetTip(const std::u16string& tip_text,
              LearnMoreLinkClickedCallback learn_more_link_clicked_callback =
                  LearnMoreLinkClickedCallback());

  // Clears the tip and hides the view.
  void ClearTip();

  // views::View:
  gfx::Size GetMinimumSize() const override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  void Layout() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;

  static constexpr int kTipMarqueeIconSize = 16;
  static constexpr int kTipMarqueeIconPadding = 6;
  static constexpr int kTipMarqueeIconTotalWidth =
      kTipMarqueeIconSize + kTipMarqueeIconPadding;

 private:
  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  void LinkClicked();
  bool GetFitsInLayout() const;

  bool IsPointInIcon(const gfx::Point& p) const;

  void ToggleOverflowWidget();

  std::u16string tip_text_;
  views::StyledLabel* tip_text_label_ = nullptr;
  LearnMoreLinkClickedCallback learn_more_link_clicked_callback_;
  bool collapsed_ = false;
  views::Widget* show_tip_widget_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_TIP_MARQUEE_VIEW_H_
