// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPLIT_TABS_SPLIT_TAB_VISUAL_DATA_H_
#define COMPONENTS_SPLIT_TABS_SPLIT_TAB_VISUAL_DATA_H_

namespace split_tabs {

// Orientation of a split view. Needs to be kept in sync with
// tab_search::mojom::SplitTabLayout in
// chrome/browser/ui/webui/tab_search/tab_search.mojom.
// LINT.IfChange(SplitTabLayout)
enum class SplitTabLayout {
  // Two tabs will display next to each other, side-by-side.
  kSideBySide,
  // Two tabs will appear stacked, one above the other.
  kStacked,
  kMaxValue = kStacked
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:SplitTabLayout)

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

#endif  // COMPONENTS_SPLIT_TABS_SPLIT_TAB_VISUAL_DATA_H_
