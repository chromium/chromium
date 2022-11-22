// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_Z_ORDERABLE_TAB_CONTAINER_ELEMENT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_Z_ORDERABLE_TAB_CONTAINER_ELEMENT_H_

#include "base/memory/raw_ptr.h"

namespace views {
class View;
}

// A class that calculates a z-value for a TabContainer child view (one of a
// tab, a tab group header, a tab group underline, or a tab group highlight).
// Can be compared with other ZOrderableTabContainerElements to determine paint
// order of their associated views.
class ZOrderableTabContainerElement {
 public:
  explicit ZOrderableTabContainerElement(views::View* const child)
      : child_(child), z_value_(CalculateZValue(child)) {}

  // Returns true iff a ZOrderableTabContainerElement can be constructed with
  // `view`.
  static bool CanOrderView(views::View* view);

  bool operator<(const ZOrderableTabContainerElement& rhs) const {
    return z_value_ < rhs.z_value_;
  }

  views::View* view() const { return child_; }

 private:
  // Determines the 'height' of |child|, which should be used to determine the
  // paint order of TabContainer's children.  Larger z-values should be painted
  // on top of smaller ones.
  static float CalculateZValue(views::View* child);

  raw_ptr<views::View> child_;
  float z_value_;
};  // ZOrderableTabContainerElement

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_Z_ORDERABLE_TAB_CONTAINER_ELEMENT_H_
