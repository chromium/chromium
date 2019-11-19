// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_LAYOUT_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_LAYOUT_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/chrome_typography_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"

enum ChromeInsetsMetric {
  // Padding around buttons on the bookmarks bar.
  INSETS_BOOKMARKS_BAR_BUTTON = views::VIEWS_INSETS_END,
  // Margins used by toasts.
  INSETS_TOAST,
};

enum ChromeDistanceMetric {
  // Default minimum width of a button.
  DISTANCE_BUTTON_MINIMUM_WIDTH = views::VIEWS_DISTANCE_END,
  // Vertical spacing at the beginning and end of a content list (a vertical
  // stack of composite views that behaves like a menu) containing one item.
  DISTANCE_CONTENT_LIST_VERTICAL_SINGLE,
  // Same as |DISTANCE_CONTENT_LIST_VERTICAL_SINGLE|, but used at the beginning
  // and end of a multi-item content list.
  DISTANCE_CONTENT_LIST_VERTICAL_MULTI,
  // Vertical spacing between a list of multiple controls in one column.
  DISTANCE_CONTROL_LIST_VERTICAL,
  // Width of the space in a dropdown button between its label and down arrow.
  DISTANCE_DROPDOWN_BUTTON_LABEL_ARROW_SPACING,
  // Width of the horizontal padding in a dropdown button between the down arrow
  // and the button's border.
  DISTANCE_DROPDOWN_BUTTON_RIGHT_MARGIN,
  // Smaller horizontal spacing between other controls that are logically
  // related.
  DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL,
  // Smaller vertical spacing between controls that are logically related.
  DISTANCE_RELATED_CONTROL_VERTICAL_SMALL,
  // Horizontal spacing between an item and the related label, in the context of
  // a row of such items. E.g. the bookmarks bar.
  DISTANCE_RELATED_LABEL_HORIZONTAL_LIST,
  // Horizontal indent of a subsection relative to related items above, e.g.
  // checkboxes below explanatory text/headings.
  DISTANCE_SUBSECTION_HORIZONTAL_INDENT,
  // Vertical margin for controls in a toast.
  DISTANCE_TOAST_CONTROL_VERTICAL,
  // Vertical margin for labels in a toast.
  DISTANCE_TOAST_LABEL_VERTICAL,
  // Horizontal spacing between controls that are logically unrelated.
  DISTANCE_UNRELATED_CONTROL_HORIZONTAL,
  // Larger horizontal spacing between unrelated controls.
  DISTANCE_UNRELATED_CONTROL_HORIZONTAL_LARGE,
  // Larger vertical spacing between unrelated controls.
  DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE,
  // Width of modal dialogs unless the content is too wide to make that
  // feasible.
  DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH,
  // Width of larger modal dialogs that require extra width.
  DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH,
  // Width of a bubble attached to the tabstrip.
  DISTANCE_BUBBLE_TABSTRIP_PREFERRED_WIDTH,
  // Width of a bubble unless the content is too wide to make that
  // feasible.
  DISTANCE_BUBBLE_PREFERRED_WIDTH,
  // Width of a bubble that appears mid-screen (like a standalone dialog)
  // instead of being anchored.
  DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH,
  // Horizontal spacing between value and description in the row.
  DISTANCE_BETWEEN_PRIMARY_AND_SECONDARY_LABELS_HORIZONTAL
};

class ChromeLayoutProvider : public views::LayoutProvider {
 public:
  ChromeLayoutProvider();
  ~ChromeLayoutProvider() override;

  static ChromeLayoutProvider* Get();
  static std::unique_ptr<views::LayoutProvider> CreateLayoutProvider();

  // views::LayoutProvider:
  gfx::Insets GetInsetsMetric(int metric) const override;
  int GetDistanceMetric(int metric) const override;
  int GetSnappedDialogWidth(int min_width) const override;
  const views::TypographyProvider& GetTypographyProvider() const override;
  gfx::ShadowValues MakeShadowValues(int elevation,
                                     SkColor color) const override;

  // Returns the alignment used for control labels in a GridLayout; for example,
  // in this GridLayout:
  //   ---------------------------
  //   | Label 1      Checkbox 1 |
  //   | Label 2      Checkbox 2 |
  //   ---------------------------
  // This value controls the alignment used for "Label 1" and "Label 2".
  virtual views::GridLayout::Alignment GetControlLabelGridAlignment() const;

  // Returns whether to show the icon next to the title text on a dialog.
  virtual bool ShouldShowWindowIcon() const;

 private:
  const ChromeTypographyProvider typography_provider_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLayoutProvider);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_LAYOUT_PROVIDER_H_
