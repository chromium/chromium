// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"

class LocationBarView;

// Use a LocationIconView to display an icon on the leading side of the edit
// field. It shows the user's current action (while the user is editing), or the
// page security status (after navigation has completed), or extension name (if
// the URL is a chrome-extension:// URL).
class LocationIconView : public IconLabelBubbleView {
 public:
  LocationIconView(const gfx::FontList& font_list,
                   LocationBarView* location_bar);
  ~LocationIconView() override;

  // IconLabelBubbleView:
  gfx::Size GetMinimumSize() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;
  SkColor GetTextColor() const override;
  bool ShouldShowSeparator() const override;
  bool ShouldShowExtraEndSpace() const override;
  bool ShowBubble(const ui::Event& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool IsBubbleShowing() const override;
  SkColor GetInkDropBaseColor() const override;

  // Whether we should show the tooltip for this icon or not.
  void set_show_tooltip(bool show_tooltip) { show_tooltip_ = show_tooltip; }

  // Returns what the minimum size would be if the label text were |text|.
  gfx::Size GetMinimumSizeForLabelText(const base::string16& text) const;

  const gfx::FontList& GetFontList() const { return font_list(); }

  // Sets whether the text should be visible. |should_animate| controls whether
  // any necessary transition to this state should be animated.
  void SetTextVisibility(bool should_show, bool should_animate);

  // Updates the icon's ink drop mode and focusable behavior.
  void Update();

 protected:
  // IconLabelBubbleView:
  bool IsTriggerableEvent(const ui::Event& event) override;
  double WidthMultiplier() const override;

 private:
  // Returns what the minimum size would be if the preferred size were |size|.
  gfx::Size GetMinimumSizeForPreferredSize(gfx::Size size) const;

  int GetSlideDurationTime() const override;

  // True if hovering this view should display a tooltip.
  bool show_tooltip_;

  LocationBarView* location_bar_;

  DISALLOW_COPY_AND_ASSIGN(LocationIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_VIEW_H_
