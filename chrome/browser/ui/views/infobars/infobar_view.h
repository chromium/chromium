// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_container.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/focus/external_focus_tracker.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class ImageView;
class Label;
class Link;
class MenuRunner;
}  // namespace views

class InfoBarView : public infobars::InfoBar,
                    public views::View,
                    public views::ExternalFocusTracker {
  METADATA_HEADER(InfoBarView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kInfoBarElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDismissButtonElementId);

  explicit InfoBarView(std::unique_ptr<infobars::InfoBarDelegate> delegate);
  InfoBarView(const InfoBarView&) = delete;
  InfoBarView& operator=(const InfoBarView&) = delete;
  ~InfoBarView() override;

  // views::View:
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  void OnThemeChanged() override;

  // views::ExternalFocusTracker:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override;

 protected:
  using Labels = std::vector<views::Label*>;

  // Creates a label with the appropriate font and color for an infobar.
  std::unique_ptr<views::Label> CreateLabel(const std::u16string& text) const;

  // Creates a link with the appropriate font and color for an infobar.
  // NOTE: Subclasses must ignore link clicks if we're unowned.
  std::unique_ptr<views::Link> CreateLink(const std::u16string& text);

  // Given |views| and the total |available_width| to display them in, sets
  // each view's size so that the longest view shrinks until it reaches the
  // length of the next-longest view, then both shrink until reaching the
  // length of the next-longest, and so forth.
  static void AssignWidths(Views* views, int available_width);

  // Returns the minimum width the content (that is, everything between the icon
  // and the close button) can be shrunk to.  This is used to prevent the close
  // button from overlapping views that cannot be shrunk any further.
  virtual int GetContentMinimumWidth() const;

  // These return x coordinates delimiting the usable area for subclasses to lay
  // out their controls.
  int GetStartX() const;
  int GetEndX() const;

  // Given a |view|, returns the centered y position, taking into account
  // animation so the control "slides in" (or out) as we animate open and
  // closed.
  int OffsetY(views::View* view) const;

  // infobars::InfoBar:
  void PlatformSpecificShow(bool animate) override;
  void PlatformSpecificHide(bool animate) override;
  void PlatformSpecificOnHeightRecalculated() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(InfoBarViewTest, GetDrawSeparator);

  // Does the actual work for AssignWidths().  Assumes |views| is sorted by
  // decreasing preferred width.
  static void AssignWidthsSorted(Views* views, int available_width);

  // Sets various attributes on |label| that are common to all child links and
  // labels.
  void SetLabelDetails(views::Label* label) const;

  // Callback used by the link created by CreateLink().
  void LinkClicked(const ui::Event& event);

  void CloseButtonPressed();

  // The optional icon at the left edge of the InfoBar.
  raw_ptr<views::ImageView> icon_ = nullptr;

  // The close button at the right edge of the InfoBar.
  raw_ptr<views::ImageButton> close_button_ = nullptr;

  // Used to run the menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_
