// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_

#include <memory>
#include <optional>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_container.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
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
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kLeftBalancerElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kRightSpacerElementId);

  explicit InfoBarView(std::unique_ptr<infobars::InfoBarDelegate> delegate);
  InfoBarView(const InfoBarView&) = delete;
  InfoBarView& operator=(const InfoBarView&) = delete;
  ~InfoBarView() override;

  // views::View:
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnThemeChanged() override;

  // views::ExternalFocusTracker:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override;

  views::ImageView* icon() { return icon_; }
  views::View* close_button() { return close_button_.get(); }

 protected:
  using Labels = std::vector<views::Label*>;

  // Creates a label with the appropriate font and color for an infobar.
  std::unique_ptr<views::Label> CreateLabel(const std::u16string& text) const;

  // Creates a link with the appropriate font and color for an infobar.
  // By default, `text` will be used as a accessible text if it's not explicitly
  // provided. NOTE: Subclasses must ignore link clicks if we're unowned.
  std::unique_ptr<views::Link> CreateLink(
      const std::u16string& text,
      const std::optional<std::u16string>& accessible_text = std::nullopt);

  // Given |views| and the total |available_width| to display them in, sets
  // each view's size so that the longest view shrinks until it reaches the
  // length of the next-longest view, then both shrink until reaching the
  // length of the next-longest, and so forth.
  static void AssignWidths(Views* views, int available_width);

  // Returns the minimum width the content (that is, everything between the icon
  // and the close button) can be shrunk to.  This is used to prevent the close
  // button from overlapping views that cannot be shrunk any further.
  virtual int GetContentMinimumWidth() const;

  // Returns the preferred width the content (that is, everything between the
  // icon and the close button).
  virtual int GetContentPreferredWidth() const;

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

  // Adds `child` to `content_container_` and returns the raw pointer. All
  // subclasses should call this instead of AddChildView().
  template <typename ViewClass>
  ViewClass* AddContentChildView(std::unique_ptr<ViewClass> child) {
    CHECK(content_container_);
    return content_container_->AddChildView(std::move(child));
  }
  // Allow subclasses to configure the content container.
  views::View* content_container() { return content_container_.get(); }

  // Adds a view to the infobar's root, placing it just before the
  // close button.
  void AddViewBeforeCloseButton(std::unique_ptr<views::View> view);

 private:
  FRIEND_TEST_ALL_PREFIXES(InfoBarViewTest, GetDrawSeparator);

  // Recalculates the layout of the infobar's contents, ensuring that views are
  // balanced to fit within the available space inside the infobar container.
  void RecalculateLayoutBalancing();

  // Make all AddChildView* overloads private so downstream subclasses
  // cannot call them directly.
  using View::AddChildView;
  using View::AddChildViewAt;
  using View::AddChildViewRaw;

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

  // Container that owns all views that should appear between `icon_` and
  // `close_button_`.  Subclasses **must** add their children to this view via
  // the AddContentChildView() helper; doing so guarantees that the close
  // button always stays the last child without any explicit re‑ordering.
  raw_ptr<views::View> content_container_ = nullptr;

  // The close button at the right edge of the InfoBar.
  raw_ptr<views::ImageButton> close_button_ = nullptr;

  // Used to run the menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The left balancer view for flex layout.
  raw_ptr<views::View> left_balancer_ = nullptr;
  // The right spacer view for flex layout.
  raw_ptr<views::View> right_spacer_ = nullptr;
  // The right side container view for flex layout.
  raw_ptr<views::View> right_side_container_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_VIEW_H_
