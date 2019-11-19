// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_VISUAL_DATA_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_VISUAL_DATA_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"

class TabGroupVisualData {
 public:
  // Construct a TabGroupVisualData with placeholder name and random color.
  TabGroupVisualData();

  TabGroupVisualData(base::string16 title, SkColor color);

  TabGroupVisualData(const TabGroupVisualData& other) = default;
  TabGroupVisualData(TabGroupVisualData&& other) = default;

  TabGroupVisualData& operator=(const TabGroupVisualData& other) = default;
  TabGroupVisualData& operator=(TabGroupVisualData&& other) = default;

  base::string16 title() const { return title_; }
  SkColor color() const { return color_; }

  // Checks whether two instances are visually equivalent.
  bool operator==(const TabGroupVisualData& other) const {
    return title_ == other.title_ && color_ == other.color_;
  }
  bool operator!=(const TabGroupVisualData& other) const {
    return !(*this == other);
  }

 private:
  base::string16 title_;
  SkColor color_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_VISUAL_DATA_H_
