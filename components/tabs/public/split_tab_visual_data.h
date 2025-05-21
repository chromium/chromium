// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_SPLIT_TAB_VISUAL_DATA_H_
#define COMPONENTS_TABS_PUBLIC_SPLIT_TAB_VISUAL_DATA_H_

namespace split_tabs {

enum class SplitTabLayout {
  // A tab will stretch out vertically so one tab in the split will be next to
  // the other.
  kVertical,
  // A tab will stretch out horizontally so one tab in the split will be on top
  // of the other.
  kHorizontal
};

// Represents the visual state of a split tab, including its layout type and the
// proportional size of the webcontents.
class SplitTabVisualData {
 public:
  SplitTabVisualData();
  explicit SplitTabVisualData(SplitTabLayout split_layout);
  SplitTabVisualData(SplitTabLayout split_layout, double split_ratio);
  ~SplitTabVisualData();

  SplitTabVisualData(const SplitTabVisualData& other) = default;
  SplitTabVisualData(SplitTabVisualData&& other) = default;

  SplitTabVisualData& operator=(const SplitTabVisualData& other) = default;
  SplitTabVisualData& operator=(SplitTabVisualData&& other) = default;

  void set_split_layout(SplitTabLayout split_layout) {
    split_layout_ = split_layout;
  }

  void set_split_ratio(double split_ratio) { split_ratio_ = split_ratio; }

  SplitTabLayout split_layout() const { return split_layout_; }

  double split_ratio() const { return split_ratio_; }

  // Checks whether two instances are visually equivalent.
  friend bool operator==(const SplitTabVisualData&,
                         const SplitTabVisualData&) = default;

 private:
  SplitTabLayout split_layout_;
  // ratio of the first split tab's width to the available width.
  double split_ratio_ = 0.5;
};

}  // namespace split_tabs

#endif  // COMPONENTS_TABS_PUBLIC_SPLIT_TAB_VISUAL_DATA_H_
