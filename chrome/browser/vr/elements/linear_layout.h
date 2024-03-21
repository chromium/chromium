// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_LINEAR_LAYOUT_H_
#define CHROME_BROWSER_VR_ELEMENTS_LINEAR_LAYOUT_H_

#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/vr_ui_export.h"

namespace vr {

class VR_UI_EXPORT LinearLayout : public UiElement {
 public:
  enum Direction { kUp, kDown, kLeft, kRight };

  explicit LinearLayout(Direction direction);
  ~LinearLayout() override;

  void set_margin(float margin) { margin_ = margin; }
  void set_direction(Direction direction) { direction_ = direction; }

  // UiElement overrides.
  void LayOutContributingChildren() override;

 private:
  bool Horizontal() const;

  // Compute the total extents of all layout-enabled children, including margin.
  // Optionally, an element to exclude may be specified, allowing the layout to
  // compute how much space is left for that element.
  void GetTotalExtent(const UiElement* element_to_exclude,
                      float* major_extent,
                      float* minor_extent) const;

  Direction direction_;

  // The spacing between elements.
  float margin_ = 0.0f;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_LINEAR_LAYOUT_H_
