// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_NEW_BADGE_LABEL_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_NEW_BADGE_LABEL_H_

#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/new_badge.h"
#include "ui/views/style/typography.h"

// Extends views::Label to display a "New" badge next to the text drawing
// attention to a new feature in Chrome. You should be able to substitute this
// anywhere you have a label - e.g. in a dialog with a new option or feature.
class NewBadgeLabel : public views::Label {
 public:
  // Determines how the badge is placed relative to the label text if the label
  // is wider than its preferred size (has no effect otherwise).
  enum class BadgePlacement {
    // Places the "New" badge immediately after the label text (default).
    kImmediatelyAfterText,
    // Places the "New" badge all the way at the trailing edge of the control,
    // which is the right edge for LTR and the left edge for RTL.
    kTrailingEdge
  };

  METADATA_HEADER(NewBadgeLabel);

  // Constructs a new badge label. Designed to be argument-compatible with the
  // views::Label constructor so they can be substituted.
  explicit NewBadgeLabel(const base::string16& text = base::string16(),
                         int text_context = views::style::CONTEXT_LABEL,
                         int text_style = views::style::STYLE_PRIMARY,
                         gfx::DirectionalityMode directionality_mode =
                             gfx::DirectionalityMode::DIRECTIONALITY_FROM_TEXT);
  NewBadgeLabel(const base::string16& text, const CustomFont& font);
  ~NewBadgeLabel() override;

  bool GetPadAfterNewBadge() const { return pad_after_new_badge_; }
  void SetPadAfterNewBadge(bool pad_after_new_badge);

  BadgePlacement GetBadgePlacement() const { return badge_placement_; }
  void SetBadgePlacement(BadgePlacement badge_placement);

  // Label:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  int GetHeightForWidth(int w) const override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  // Add the required internal padding to the label so that there is room to
  // display the new badge.
  void UpdatePaddingForNewBadge();

  // Specifies the placement of the "New" badge when the label is wider than its
  // preferred size.
  BadgePlacement badge_placement_ = BadgePlacement::kImmediatelyAfterText;

  // Determines whether there is additional internal margin to the right of the
  // "New" badge. When set to true, the space will be allocated, and
  // kInternalPaddingKey will be set so that layouts know this space is empty.
  bool pad_after_new_badge_ = true;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_NEW_BADGE_LABEL_H_
