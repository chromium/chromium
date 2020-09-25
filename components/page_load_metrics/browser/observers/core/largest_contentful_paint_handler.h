// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_LARGEST_CONTENTFUL_PAINT_HANDLER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_LARGEST_CONTENTFUL_PAINT_HANDLER_H_

#include <map>

#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/page_load_timing.h"

namespace content {

class NavigationHandle;
class RenderFrameHost;

}  // namespace content

namespace page_load_metrics {

class ContentfulPaintTimingInfo {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class LargestContentType {
    kImage = 0,
    kText = 1,
    kMaxValue = kText,
  };

  explicit ContentfulPaintTimingInfo(LargestContentType largest_content_type,
                                     bool in_main_frame);
  explicit ContentfulPaintTimingInfo(
      const base::Optional<base::TimeDelta>&,
      const uint64_t& size,
      const LargestContentType largest_content_type,
      bool in_main_frame);
  explicit ContentfulPaintTimingInfo(const ContentfulPaintTimingInfo& other);
  void Reset(const base::Optional<base::TimeDelta>&, const uint64_t& size);
  base::Optional<base::TimeDelta> Time() const { return time_; }
  bool InMainFrame() const { return in_main_frame_; }
  uint64_t Size() const { return size_; }
  LargestContentType Type() const { return type_; }

  // Returns true iff this object does not represent any paint.
  bool Empty() const {
    // size_ and time_ should both be set or both be unset.
    DCHECK((size_ != 0u && time_) || (size_ == 0u && !time_));
    return !time_;
  }

  // Returns true iff this object does not represent any paint OR represents an
  // image that has not finished loading.
  bool ContainsValidTime() const {
    return time_ && *time_ != base::TimeDelta();
  }

  std::unique_ptr<base::trace_event::TracedValue> DataAsTraceValue() const;

 private:
  ContentfulPaintTimingInfo() = delete;
  std::string TypeInString() const;
  base::Optional<base::TimeDelta> time_;
  uint64_t size_;
  LargestContentType type_;
  bool in_main_frame_;
};

class ContentfulPaint {
 public:
  explicit ContentfulPaint(bool in_main_frame);
  ContentfulPaintTimingInfo& Text() { return text_; }
  const ContentfulPaintTimingInfo& Text() const { return text_; }
  ContentfulPaintTimingInfo& Image() { return image_; }
  const ContentfulPaintTimingInfo& Image() const { return image_; }
  const ContentfulPaintTimingInfo& MergeTextAndImageTiming() const;

 private:
  ContentfulPaintTimingInfo text_;
  ContentfulPaintTimingInfo image_;
};

class LargestContentfulPaintHandler {
 public:
  using FrameTreeNodeId = int;
  static void SetTestMode(bool enabled);
  LargestContentfulPaintHandler();
  ~LargestContentfulPaintHandler();

  // Returns true if the out parameters are assigned values.
  static bool AssignTimeAndSizeForLargestContentfulPaint(
      const page_load_metrics::mojom::LargestContentfulPaintTiming&
          largest_contentful_paint,
      base::Optional<base::TimeDelta>* largest_content_paint_time,
      uint64_t* largest_content_paint_size,
      ContentfulPaintTimingInfo::LargestContentType* largest_content_type);

  void RecordTiming(
      const page_load_metrics::mojom::LargestContentfulPaintTiming&
          largest_contentful_paint,
      const base::Optional<base::TimeDelta>&
          first_input_or_scroll_notified_timestamp,
      content::RenderFrameHost* subframe_rfh);
  inline void RecordMainFrameTreeNodeId(int main_frame_tree_node_id) {
    main_frame_tree_node_id_.emplace(main_frame_tree_node_id);
  }

  inline int MainFrameTreeNodeId() const {
    return main_frame_tree_node_id_.value();
  }

  // We merge the candidates from text side and image side to get the largest
  // candidate across both types of content.
  const ContentfulPaintTimingInfo& MainFrameLargestContentfulPaint() const {
    return main_frame_contentful_paint_.MergeTextAndImageTiming();
  }
  const ContentfulPaintTimingInfo& SubframesLargestContentfulPaint() const {
    return subframe_contentful_paint_.MergeTextAndImageTiming();
  }
  const ContentfulPaintTimingInfo& MainFrameLargestImagePaint() const {
    return main_frame_contentful_paint_.Image();
  }
  const ContentfulPaintTimingInfo& MainFrameLargestTextPaint() const {
    return main_frame_contentful_paint_.Text();
  }

  // We merge the candidates from main frame and subframe to get the largest
  // candidate across all frames.
  const ContentfulPaintTimingInfo& MergeMainFrameAndSubframes() const;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle,
      base::TimeTicks navigation_start);
  void OnFrameDeleted(content::RenderFrameHost* render_frame_host);

 private:
  void RecordSubframeTiming(
      const page_load_metrics::mojom::LargestContentfulPaintTiming&
          largest_contentful_paint,
      const base::Optional<base::TimeDelta>&
          first_input_or_scroll_notified_timestamp,
      const base::TimeDelta& navigation_start_offset);
  void RecordMainFrameTiming(
      const page_load_metrics::mojom::LargestContentfulPaintTiming&
          largest_contentful_paint,
      const base::Optional<base::TimeDelta>&
          first_input_or_scroll_notified_timestamp);
  void UpdateFirstInputOrScrollNotified(
      const base::Optional<base::TimeDelta>& candidate_new_time,
      const base::TimeDelta& navigation_start_offset);
  void MergeForSubframes(
      ContentfulPaintTimingInfo* inout_timing,
      const base::Optional<base::TimeDelta>& candidate_new_time,
      const uint64_t& candidate_new_size,
      base::TimeDelta navigation_start_offset);
  bool IsValid(const base::Optional<base::TimeDelta>& time) {
    // When |time| is not present, this means that there is no current
    // candidate. If |time| is 0, it corresponds to an image that has not
    // finished loading. In both cases, we do not know the timestamp at which
    // |time| was determned. Therefore, we just assume that the time is valid
    // only if we have not yet received a notification for first input or scroll
    if (!time.has_value() || (*time).is_zero())
      return first_input_or_scroll_notified_ == base::TimeDelta::Max();
    return *time < first_input_or_scroll_notified_;
  }
  ContentfulPaint main_frame_contentful_paint_;
  ContentfulPaint subframe_contentful_paint_;

  // Used for Telemetry to distinguish the LCP events from different
  // navigations.
  base::Optional<int> main_frame_tree_node_id_;

  // The first input or scroll across all frames in the page. Used to filter out
  // paints that occur on other frames but after this time.
  base::TimeDelta first_input_or_scroll_notified_ = base::TimeDelta::Max();

  // Navigation start offsets for the most recently committed document in each
  // frame.
  std::map<FrameTreeNodeId, base::TimeDelta> subframe_navigation_start_offset_;
  DISALLOW_COPY_AND_ASSIGN(LargestContentfulPaintHandler);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_LARGEST_CONTENTFUL_PAINT_HANDLER_H_
