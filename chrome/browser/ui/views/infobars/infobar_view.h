// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_container.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/focus/external_focus_tracker.h"

namespace views {
class ImageButton;
class ImageView;
class Label;
class Link;
class LinkListener;
class MenuRunner;
}  // namespace views

class InfoBarView : public infobars::InfoBar,
                    public views::View,
                    public views::ButtonListener,
                    public views::ExternalFocusTracker {
 public:
  explicit InfoBarView(std::unique_ptr<infobars::InfoBarDelegate> delegate);
  ~InfoBarView() override;

  // Requests that the infobar recompute its target height.
  void RecalculateHeight();

  // views::View:
  void Layout() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // views::ButtonListener:
  // NOTE: This must not be called if we're unowned.  (Subclasses should ignore
  // calls to ButtonPressed() in this case.)
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::ExternalFocusTracker:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override;

 protected:
  using Labels = std::vector<views::Label*>;

  // Creates a label with the appropriate font and color for an infobar.
  views::Label* CreateLabel(const base::string16& text) const;

  // Creates a link with the appropriate font and color for an infobar.
  // NOTE: Subclasses must ignore link clicks if we're unowned.
  views::Link* CreateLink(const base::string16& text,
                          views::LinkListener* listener) const;

  // Given |views| and the total |available_width| to display them in, sets
  // each view's size so that the longest view shrinks until it reaches the
  // length of the next-longest view, then both shrink until reaching the
  // length of the next-longest, and so forth.
  static void AssignWidths(Views* views, int available_width);

  // Returns the minimum width the content (that is, everything between the icon
  // and the close button) can be shrunk to.  This is used to prevent the close
  // button from overlapping views that cannot be shrunk any further.
  virtual int ContentMinimumWidth() const;

  // These return x coordinates delimiting the usable area for subclasses to lay
  // out their controls.
  int StartX() const;
  int EndX() const;

  // Given a |view|, returns the centered y position, taking into account
  // animation so the control "slides in" (or out) as we animate open and
  // closed.
  int OffsetY(views::View* view) const;

  // infobars::InfoBar:
  void PlatformSpecificShow(bool animate) override;
  void PlatformSpecificHide(bool animate) override;
  void PlatformSpecificOnHeightRecalculated() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(InfoBarViewTest, ShouldDrawSeparator);

  // Does the actual work for AssignWidths().  Assumes |views| is sorted by
  // decreasing preferred width.
  static void AssignWidthsSorted(Views* views, int available_width);

  // Returns whether this infobar should draw a 1 px separator at its top.
  bool ShouldDrawSeparator() const;

  // Returns how much space the container should reserve for a separator between
  // infobars, in addition to the height of the infobars themselves.
  int GetSeparatorHeight() const;

  // Returns the current color for the theme property |id|.  Will return the
  // wrong value if no theme provider is available.
  SkColor GetColor(int id) const;

  // Sets various attributes on |label| that are common to all child links and
  // labels.
  void SetLabelDetails(views::Label* label) const;

  // The optional icon at the left edge of the InfoBar.
  views::ImageView* icon_ = nullptr;

  // The close button at the right edge of the InfoBar.
  views::ImageButton* close_button_ = nullptr;

  // Used to run the menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_
