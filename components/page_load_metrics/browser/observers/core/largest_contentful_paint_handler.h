// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_LARGEST_CONTENTFUL_PAINT_HANDLER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_LARGEST_CONTENTFUL_PAINT_HANDLER_H_

#include <map>
#include <optional>

#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "url/gurl.h"

namespace content {

class NavigationHandle;
class RenderFrameHost;

}  // namespace content

namespace page_load_metrics {

class ContentfulPaintTimingInfo {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class LargestContentTextOrImage {
    kImage = 0,
    kText = 1,
    kMaxValue = kText,
  };

  ContentfulPaintTimingInfo(LargestContentTextOrImage largest_content_type,
                            bool in_main_frame,
                            blink::LargestContentfulPaintType type);
  ContentfulPaintTimingInfo(
      const std::optional<base::TimeDelta>&,
      const uint64_t& size,
      const LargestContentTextOrImage largest_content_type,
      double image_bpp,
      const std::optional<net::RequestPriority>& image_request_priority,
      bool in_main_frame,
      const blink::LargestContentfulPaintType type,
      const std::optional<base::TimeDelta>& image_discovery_time,
      const std::optional<base::TimeDelta>& image_load_start,
      const std::optional<base::TimeDelta>& image_load_end);
  ContentfulPaintTimingInfo(const ContentfulPaintTimingInfo& other);
  void Reset(const std::optional<base::TimeDelta>& time,
             const uint64_t& size,
             blink::LargestContentfulPaintType type,
             double image_bpp,
             const std::optional<net::RequestPriority>& image_request_priority,
             const std::optional<base::TimeDelta>& image_discovery_time,
             const std::optional<base::TimeDelta>& image_load_start,
             const std::optional<base::TimeDelta>& image_load_end);
  std::optional<base::TimeDelta> Time() const { return time_; }
  std::optional<base::TimeDelta> ImageDiscoveryTime() const {
    return image_discovery_time_;
  }
  std::optional<base::TimeDelta> ImageLoadStart() const {
    return image_load_start_;
  }
  std::optional<base::TimeDelta> ImageLoadEnd() const {
    return image_load_end_;
  }

  bool InMainFrame() const { return in_main_frame_; }
  blink::LargestContentfulPaintType Type() const { return type_; }
  uint64_t Size() const { return size_; }
  LargestContentTextOrImage TextOrImage() const { return text_or_image_; }
  double ImageBPP() const { return image_bpp_; }
  std::optional<net::RequestPriority> ImageRequestPriority() const {
    return image_request_priority_;
  }

  // Returns true iff this object does not represent any paint.
  bool Empty() const {
    // We set timings at the renderer side only when size is >0. Therefore it
    // could be either size == 0 and time is not set or size > 0 and time is
    // set. Note that when time is set, it could be 0.
    CHECK((size_ != 0u && time_.has_value()) ||
          (size_ == 0u && !time_.has_value()));

    // Returns if timing is not set because of 0 size.
    // TODO(crbug.com/40926935) We should revisit if we should check timing
    // being 0 too.
    return (size_ == 0u && !time_.has_value());
  }

  // Returns true iff this object does not represent any paint OR represents an
  // image that has not finished loading.
  bool ContainsValidTime() const {
    return time_ && *time_ != base::TimeDelta();
  }

  std::unique_ptr<base::trace_event::TracedValue> DataAsTraceValue() const;

  ContentfulPaintTimingInfo() = delete;

 private:
  std::string TextOrImageInString() const;
  std::optional<base::TimeDelta> time_;
  uint64_t size_;
  LargestContentTextOrImage text_or_image_;
  blink::LargestContentfulPaintType type_ =
      blink::LargestContentfulPaintType::kNone;
  double image_bpp_ = 0.0;
  std::optional<net::RequestPriority> image_request_priority_;
  bool in_main_frame_;
  std::optional<base::TimeDelta> image_discovery_time_;
  std::optional<base::TimeDelta> image_load_start_;
  std::optional<base::TimeDelta> image_load_end_;
};

class ContentfulPaint {
 public:
  explicit ContentfulPaint(bool in_main_frame,
                           blink::LargestContentfulPaintType type);
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
  static void SetTestMode(bool enabled);
  LargestContentfulPaintHandler();

  LargestContentfulPaintHandler(const LargestContentfulPaintHandler&) = delete;
  LargestContentfulPaintHandler& operator=(
      const LargestContentfulPaintHandler&) = delete;

  ~LargestContentfulPaintHandler();

  // Returns true if the out parameters are assigned values.
  static bool AssignTimeAndSizeForLargestContentfulPaint(
      const page_load_metrics::mojom::LargestContentfulPaintTiming&
          largest_contentful_paint,
      std::optional<base::TimeDelta>* largest_content_paint_time,
      uint64_t* largest_content_paint_size,
      ContentfulPaintTimingInfo::LargestContentTextOrImage*
          largest_content_type);

  void RecordMainFrameTiming(
      const page_load_metrics::mojom::LargestContentfulPaintTiming&
          largest_contentful_paint,
      const std::optional<base::TimeDelta>&
          first_input_or_scroll_notified_timestamp);
  void RecordSubFrameTiming(
      const page_load_metrics::mojom::LargestContentfulPaintTiming&
          largest_contentful_paint,
      const std::optional<base::TimeDelta>&
          first_input_or_scroll_notified_timestamp,
      content::RenderFrameHost* subframe_rfh,
      const GURL& main_frame_url);
  inline void RecordMainFrameTreeNodeId(
      content::FrameTreeNodeId main_frame_tree_node_id) {
    main_frame_tree_node_id_.emplace(main_frame_tree_node_id);
  }

  inline content::FrameTreeNodeId MainFrameTreeNodeId() const {
    return main_frame_tree_node_id_.value();
  }

  // We merge the candidates from text side and image side to get the largest
  // candidate across both types of content.
  const ContentfulPaintTimingInfo& MainFrameLargestContentfulPaint() const {
    return main_frame_contentful_paint_.MergeTextAndImageTiming();
  }
  const ContentfulPaintTimingInfo& CrossSiteSubframesLargestContentfulPaint()
      const {
    return cross_site_subframe_contentful_paint_.MergeTextAndImageTiming();
  }

  // We merge the candidates from main frame and subframe to get the largest
  // candidate across all frames.
  const ContentfulPaintTimingInfo& MergeMainFrameAndSubframes() const;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle,
      base::TimeTicks navigation_start);
  void OnSubFrameDeleted(content::FrameTreeNodeId frame_tree_node_id);

  void UpdateSoftNavigationLargestContentfulPaint(
      const page_load_metrics::mojom::LargestContentfulPaintTiming&);

  const ContentfulPaintTimingInfo& GetSoftNavigationLargestContentfulPaint()
      const {
    return soft_navigation_contentful_paint_candidate_
        .MergeTextAndImageTiming();
  }

 private:
  void UpdateSubFrameTiming(
      const page_load_metrics::mojom::LargestContentfulPaintTiming&
          largest_contentful_paint,
      ContentfulPaint& subframe_contentful_paint,
      const std::optional<base::TimeDelta>&
          first_input_or_scroll_notified_timestamp,
      const base::TimeDelta& navigation_start_offset,
      const bool is_cross_site);
  void UpdateFirstInputOrScrollNotified(
      const std::optional<base::TimeDelta>& candidate_new_time,
      const base::TimeDelta& navigation_start_offset);
  bool IsValid(const std::optional<base::TimeDelta>& time) {
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
  // `cross_site_subframe_contentful_paint_` keeps track of the most plausible
  // LCP candidate computed from the cross-site subframes.
  ContentfulPaint cross_site_subframe_contentful_paint_;

  // Keeps track of the LCP candidate of a soft navigation.
  ContentfulPaint soft_navigation_contentful_paint_candidate_;

  // Used for Telemetry to distinguish the LCP events from different
  // navigations.
  std::optional<content::FrameTreeNodeId> main_frame_tree_node_id_;

  // The first input or scroll across all frames in the page. Used to filter out
  // paints that occur on other frames but after this time.
  base::TimeDelta first_input_or_scroll_notified_ = base::TimeDelta::Max();

  // Navigation start offsets for the most recently committed document in each
  // frame.
  std::map<content::FrameTreeNodeId, base::TimeDelta>
      subframe_navigation_start_offset_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_LARGEST_CONTENTFUL_PAINT_HANDLER_H_
