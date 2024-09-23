// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"

#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace page_load_metrics {

// TODO(crbug.com/41256933): True in test only. Since we are unable to config
// navigation start in tests, we disable the offsetting to make the test
// deterministic.
static bool g_disable_subframe_navigation_start_offset = false;

namespace {

std::optional<base::TimeDelta> AdjustedTime(
    std::optional<base::TimeDelta> candidate_time,
    base::TimeDelta navigation_start_offset) {
  // If |candidate_time| is not positive, this means that the candidate is an
  // image that has not finished loading. Preserve its meaning by not adding the
  // |navigation_start_offset|.
  std::optional<base::TimeDelta> new_time = std::nullopt;
  if (candidate_time) {
    new_time = candidate_time->is_positive()
                   ? navigation_start_offset + candidate_time.value()
                   : base::TimeDelta();
  }
  return new_time;
}

const ContentfulPaintTimingInfo& MergeTimingsBySizeAndTime(
    const ContentfulPaintTimingInfo& timing1,
    const ContentfulPaintTimingInfo& timing2) {
  // When both are empty, just return either.
  if (timing1.Empty() && timing2.Empty())
    return timing1;

  if (timing1.Empty() && !timing2.Empty())
    return timing2;
  if (!timing1.Empty() && timing2.Empty())
    return timing1;
  if (timing1.Size() > timing2.Size())
    return timing1;
  if (timing1.Size() < timing2.Size())
    return timing2;
  // When both sizes are equal
  DCHECK(timing1.Time());
  DCHECK(timing2.Time());
  // The size can be nonzero while the time can be 0 since a time of 0 is sent
  // when the image is still painting. When we merge the two
  // |ContentfulPaintTimingInfo| objects, we should ignore the one with 0 time.
  if (timing1.Time() == base::TimeDelta())
    return timing2;
  if (timing2.Time() == base::TimeDelta())
    return timing1;
  return timing1.Time().value() < timing2.Time().value() ? timing1 : timing2;
}

void MergeForSubframesWithAdjustedTime(
    ContentfulPaintTimingInfo* inout_timing,
    const ContentfulPaintTimingInfo& new_candidate) {
  DCHECK(inout_timing);
  const ContentfulPaintTimingInfo& merged_candidate =
      MergeTimingsBySizeAndTime(new_candidate, *inout_timing);
  // Image discovery time, load start/end are not reported for subframe image
  // LCP elements.
  inout_timing->Reset(
      merged_candidate.Time(), merged_candidate.Size(), merged_candidate.Type(),
      merged_candidate.ImageBPP(), merged_candidate.ImageRequestPriority(),
      merged_candidate.ImageDiscoveryTime(), merged_candidate.ImageLoadStart(),
      merged_candidate.ImageLoadEnd());
}

void Reset(ContentfulPaintTimingInfo& timing) {
  timing.Reset(std::nullopt, 0u, blink::LargestContentfulPaintType::kNone,
               /*image_bpp=*/0.0,
               /*image_request_priority=*/std::nullopt,
               /*image_discovery_time=*/std::nullopt,
               /*image_load_start=*/std::nullopt,
               /*image_load_end=*/std::nullopt);
}

bool IsSameSite(const GURL& url1, const GURL& url2) {
  // We can't use SiteInstance::IsSameSiteWithURL() because both mainframe and
  // subframe are under default SiteInstance on low-end Android environment, and
  // it treats them as same-site even though the passed url is actually not a
  // same-site.
  return url1.SchemeIs(url2.scheme()) &&
         net::registry_controlled_domains::SameDomainOrHost(
             url1, url2,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

std::optional<net::RequestPriority> GetImageRequestPriority(
    const page_load_metrics::mojom::LargestContentfulPaintTiming&
        largest_contentful_paint) {
  if (largest_contentful_paint.image_request_priority_valid)
    return largest_contentful_paint.image_request_priority_value;
  return std::nullopt;
}
}  // namespace

ContentfulPaintTimingInfo::ContentfulPaintTimingInfo(
    LargestContentTextOrImage text_or_image,
    bool in_main_frame,
    blink::LargestContentfulPaintType type)
    : size_(0),
      text_or_image_(text_or_image),
      type_(type),
      in_main_frame_(in_main_frame) {}
ContentfulPaintTimingInfo::ContentfulPaintTimingInfo(
    const std::optional<base::TimeDelta>& time,
    const uint64_t& size,
    const LargestContentTextOrImage text_or_image,
    double image_bpp,
    const std::optional<net::RequestPriority>& image_request_priority,
    bool in_main_frame,
    const blink::LargestContentfulPaintType type,
    const std::optional<base::TimeDelta>& image_discovery_time,
    const std::optional<base::TimeDelta>& image_load_start,
    const std::optional<base::TimeDelta>& image_load_end)
    : time_(time),
      size_(size),
      text_or_image_(text_or_image),
      type_(type),
      image_bpp_(image_bpp),
      image_request_priority_(image_request_priority),
      in_main_frame_(in_main_frame) {
  if (image_discovery_time.has_value()) {
    image_discovery_time_ = image_discovery_time.value();
  }
  if (image_load_start.has_value()) {
    image_load_start_ = image_load_start.value();
  }
  if (image_load_end.has_value()) {
    image_load_end_ = image_load_end.value();
  }
}

ContentfulPaintTimingInfo::ContentfulPaintTimingInfo(
    const ContentfulPaintTimingInfo& other) = default;

std::unique_ptr<base::trace_event::TracedValue>
ContentfulPaintTimingInfo::DataAsTraceValue() const {
  std::unique_ptr<base::trace_event::TracedValue> data =
      std::make_unique<base::trace_event::TracedValue>();
  data->SetInteger("durationInMilliseconds", time_.value().InMilliseconds());
  data->SetInteger("size", size_);
  data->SetString("type", TextOrImageInString());
  data->SetBoolean("inMainFrame", InMainFrame());
  data->SetBoolean(
      "isAnimated",
      (Type() & blink::LargestContentfulPaintType::kAnimatedImage) ==
          blink::LargestContentfulPaintType::kAnimatedImage);
  // The load_start and load_end are 0 for text elements.
  data->SetInteger("loadStartInMilliseconds",
                   image_load_start_.has_value()
                       ? image_load_start_.value().InMilliseconds()
                       : 0);
  data->SetInteger("loadEndInMilliseconds",
                   image_load_end_.has_value()
                       ? image_load_end_.value().InMilliseconds()
                       : 0);
  return data;
}

std::string ContentfulPaintTimingInfo::TextOrImageInString() const {
  switch (TextOrImage()) {
    case LargestContentTextOrImage::kText:
      return "text";
    case LargestContentTextOrImage::kImage:
      return "image";
    default:
      NOTREACHED_IN_MIGRATION();
      return "NOT_REACHED";
  }
}

// static
void LargestContentfulPaintHandler::SetTestMode(bool enabled) {
  g_disable_subframe_navigation_start_offset = enabled;
}

void ContentfulPaintTimingInfo::Reset(
    const std::optional<base::TimeDelta>& time,
    const uint64_t& size,
    blink::LargestContentfulPaintType type,
    double image_bpp,
    const std::optional<net::RequestPriority>& image_request_priority,
    const std::optional<base::TimeDelta>& image_discovery_time,
    const std::optional<base::TimeDelta>& image_load_start,
    const std::optional<base::TimeDelta>& image_load_end) {
  size_ = size;
  time_ = time;
  type_ = type;
  image_bpp_ = image_bpp;
  image_request_priority_ = image_request_priority;
  image_discovery_time_ = image_discovery_time;
  image_load_start_ = image_load_start;
  image_load_end_ = image_load_end;
}

ContentfulPaint::ContentfulPaint(bool in_main_frame,
                                 blink::LargestContentfulPaintType type)
    : text_(ContentfulPaintTimingInfo::LargestContentTextOrImage::kText,
            in_main_frame,
            type),
      image_(ContentfulPaintTimingInfo::LargestContentTextOrImage::kImage,
             in_main_frame,
             type) {}

const ContentfulPaintTimingInfo& ContentfulPaint::MergeTextAndImageTiming()
    const {
  return MergeTimingsBySizeAndTime(text_, image_);
}

// static
bool LargestContentfulPaintHandler::AssignTimeAndSizeForLargestContentfulPaint(
    const page_load_metrics::mojom::LargestContentfulPaintTiming&
        largest_contentful_paint,
    std::optional<base::TimeDelta>* largest_content_paint_time,
    uint64_t* largest_content_paint_size,
    ContentfulPaintTimingInfo::LargestContentTextOrImage*
        largest_content_type) {
  // Size being 0 means the paint time is not recorded.
  if (!largest_contentful_paint.largest_text_paint_size &&
      !largest_contentful_paint.largest_image_paint_size)
    return false;

  if ((largest_contentful_paint.largest_text_paint_size >
       largest_contentful_paint.largest_image_paint_size) ||
      (largest_contentful_paint.largest_text_paint_size ==
           largest_contentful_paint.largest_image_paint_size &&
       largest_contentful_paint.largest_text_paint <
           largest_contentful_paint.largest_image_paint)) {
    *largest_content_paint_time = largest_contentful_paint.largest_text_paint;
    *largest_content_paint_size =
        largest_contentful_paint.largest_text_paint_size;
    *largest_content_type =
        ContentfulPaintTimingInfo::LargestContentTextOrImage::kText;
  } else {
    *largest_content_paint_time = largest_contentful_paint.largest_image_paint;
    *largest_content_paint_size =
        largest_contentful_paint.largest_image_paint_size;
    *largest_content_type =
        ContentfulPaintTimingInfo::LargestContentTextOrImage::kImage;
  }
  return true;
}

LargestContentfulPaintHandler::LargestContentfulPaintHandler()
    : main_frame_contentful_paint_(true /*in_main_frame*/,
                                   blink::LargestContentfulPaintType::kNone),
      subframe_contentful_paint_(false /*in_main_frame*/,
                                 blink::LargestContentfulPaintType::kNone),
      cross_site_subframe_contentful_paint_(
          false /*in_main_frame*/,
          blink::LargestContentfulPaintType::kNone),
      soft_navigation_contentful_paint_candidate_(
          false,
          blink::LargestContentfulPaintType::kNone) {}

LargestContentfulPaintHandler::~LargestContentfulPaintHandler() = default;

const ContentfulPaintTimingInfo&
LargestContentfulPaintHandler::MergeMainFrameAndSubframes() const {
  const ContentfulPaintTimingInfo& main_frame_timing =
      main_frame_contentful_paint_.MergeTextAndImageTiming();
  const ContentfulPaintTimingInfo& subframe_timing =
      subframe_contentful_paint_.MergeTextAndImageTiming();
  return MergeTimingsBySizeAndTime(main_frame_timing, subframe_timing);
}

void LargestContentfulPaintHandler::UpdateSoftNavigationLargestContentfulPaint(
    const page_load_metrics::mojom::LargestContentfulPaintTiming&
        largest_contentful_paint) {
  if (largest_contentful_paint.largest_text_paint.has_value()) {
    // Image load start/end are not applicable to text LCP elements.
    soft_navigation_contentful_paint_candidate_.Text().Reset(
        largest_contentful_paint.largest_text_paint,
        largest_contentful_paint.largest_text_paint_size,
        static_cast<blink::LargestContentfulPaintType>(
            largest_contentful_paint.type),
        /*image_bpp=*/0.0,
        /*image_request_priority=*/std::nullopt,
        /*image_discovery_time=*/std::nullopt,
        /*image_load_start=*/std::nullopt,
        /*image_load_end=*/std::nullopt);
  }
  if (largest_contentful_paint.largest_image_paint.has_value()) {
    soft_navigation_contentful_paint_candidate_.Image().Reset(
        largest_contentful_paint.largest_image_paint,
        largest_contentful_paint.largest_image_paint_size,
        static_cast<blink::LargestContentfulPaintType>(
            largest_contentful_paint.type),
        largest_contentful_paint.image_bpp,
        GetImageRequestPriority(largest_contentful_paint),
        largest_contentful_paint.resource_load_timings->discovery_time,
        largest_contentful_paint.resource_load_timings->load_start,
        largest_contentful_paint.resource_load_timings->load_end);
  }
}

void LargestContentfulPaintHandler::RecordMainFrameTiming(
    const page_load_metrics::mojom::LargestContentfulPaintTiming&
        largest_contentful_paint,
    const std::optional<base::TimeDelta>&
        first_input_or_scroll_notified_timestamp) {
  UpdateFirstInputOrScrollNotified(
      first_input_or_scroll_notified_timestamp,
      /* navigation_start_offset */ base::TimeDelta());
  if (IsValid(largest_contentful_paint.largest_text_paint)) {
    // Image load start/end are not applicable to text LCP elements.
    main_frame_contentful_paint_.Text().Reset(
        largest_contentful_paint.largest_text_paint,
        largest_contentful_paint.largest_text_paint_size,
        static_cast<blink::LargestContentfulPaintType>(
            largest_contentful_paint.type),
        /*image_bpp=*/0.0,
        /*image_request_priority=*/std::nullopt,
        /*image_discovery_time=*/std::nullopt,
        /*image_load_start=*/std::nullopt,
        /*image_load_end=*/std::nullopt);
  }
  if (IsValid(largest_contentful_paint.largest_image_paint)) {
    main_frame_contentful_paint_.Image().Reset(
        largest_contentful_paint.largest_image_paint,
        largest_contentful_paint.largest_image_paint_size,
        static_cast<blink::LargestContentfulPaintType>(
            largest_contentful_paint.type),
        largest_contentful_paint.image_bpp,
        GetImageRequestPriority(largest_contentful_paint),
        largest_contentful_paint.resource_load_timings->discovery_time,
        largest_contentful_paint.resource_load_timings->load_start,
        largest_contentful_paint.resource_load_timings->load_end);
  }
}

void LargestContentfulPaintHandler::RecordSubFrameTiming(
    const page_load_metrics::mojom::LargestContentfulPaintTiming&
        largest_contentful_paint,
    const std::optional<base::TimeDelta>&
        first_input_or_scroll_notified_timestamp,
    content::RenderFrameHost* subframe_rfh,
    const GURL& main_frame_url) {
  // For subframes
  const auto it = subframe_navigation_start_offset_.find(
      subframe_rfh->GetFrameTreeNodeId());
  if (it == subframe_navigation_start_offset_.end()) {
    // We received timing information for an untracked load. Ignore it.
    return;
  }
  UpdateSubFrameTiming(largest_contentful_paint, subframe_contentful_paint_,
                       first_input_or_scroll_notified_timestamp, it->second,
                       false);
  // Note that subframe can be in other page like FencedFrames.
  // So, we can't know `main_frame_url` without help of PageLoadTracker.
  if (!IsSameSite(subframe_rfh->GetLastCommittedURL(), main_frame_url)) {
    UpdateSubFrameTiming(
        largest_contentful_paint, cross_site_subframe_contentful_paint_,
        first_input_or_scroll_notified_timestamp, it->second, true);
  }
}

// We handle subframe and main frame differently. For main frame, we directly
// substitute the candidate when we receive a new one. For subframes (plural),
// we merge the candidates from different subframes by keeping the largest one.
// Note that the merging of subframes' timings will make
// |subframe_contentful_paint_| unable to be replaced with a smaller paint (it
// should have been able when a large ephemeral element is removed). This is a
// trade-off we make to keep a simple algorithm, otherwise we will have to
// track one candidate per subframe.
void LargestContentfulPaintHandler::UpdateSubFrameTiming(
    const page_load_metrics::mojom::LargestContentfulPaintTiming&
        largest_contentful_paint,
    ContentfulPaint& subframe_contentful_paint,
    const std::optional<base::TimeDelta>&
        first_input_or_scroll_notified_timestamp,
    const base::TimeDelta& navigation_start_offset,
    const bool is_cross_site) {
  if (!is_cross_site) {
    UpdateFirstInputOrScrollNotified(first_input_or_scroll_notified_timestamp,
                                     navigation_start_offset);
  }
  DCHECK(!subframe_contentful_paint.Text().InMainFrame());
  DCHECK(!subframe_contentful_paint.Image().InMainFrame());
  ContentfulPaintTimingInfo new_text_candidate(
      AdjustedTime(largest_contentful_paint.largest_text_paint,
                   navigation_start_offset),
      largest_contentful_paint.largest_text_paint_size,
      ContentfulPaintTimingInfo::LargestContentTextOrImage::kText,
      /*image_bpp=*/0.0, /*image_request_priority=*/std::nullopt,
      /*in_main_frame=*/false,
      static_cast<blink::LargestContentfulPaintType>(
          largest_contentful_paint.type),
      /*image_discovery_time=*/std::nullopt, /*image_load_start=*/std::nullopt,
      /*image_load_end=*/std::nullopt);
  if (IsValid(new_text_candidate.Time())) {
    MergeForSubframesWithAdjustedTime(&subframe_contentful_paint.Text(),
                                      new_text_candidate);
  }

  ContentfulPaintTimingInfo new_image_candidate(
      AdjustedTime(largest_contentful_paint.largest_image_paint,
                   navigation_start_offset),
      largest_contentful_paint.largest_image_paint_size,
      ContentfulPaintTimingInfo::LargestContentTextOrImage::kImage,
      largest_contentful_paint.image_bpp,
      GetImageRequestPriority(largest_contentful_paint),
      /*in_main_frame=*/false,
      static_cast<blink::LargestContentfulPaintType>(
          largest_contentful_paint.type),
      AdjustedTime(
          largest_contentful_paint.resource_load_timings->discovery_time,
          navigation_start_offset),
      AdjustedTime(largest_contentful_paint.resource_load_timings->load_start,
                   navigation_start_offset),
      AdjustedTime(largest_contentful_paint.resource_load_timings->load_end,
                   navigation_start_offset));

  if (IsValid(new_image_candidate.Time())) {
    MergeForSubframesWithAdjustedTime(&subframe_contentful_paint.Image(),
                                      new_image_candidate);
  }
}

void LargestContentfulPaintHandler::UpdateFirstInputOrScrollNotified(
    const std::optional<base::TimeDelta>& candidate_new_time,
    const base::TimeDelta& navigation_start_offset) {
  if (!candidate_new_time.has_value())
    return;

  if (first_input_or_scroll_notified_ >
      navigation_start_offset + *candidate_new_time) {
    first_input_or_scroll_notified_ =
        navigation_start_offset + *candidate_new_time;
    // Consider candidates after input to be invalid. This is needed because
    // IPCs from different frames can arrive out of order. For example, this is
    // consistently the case when a click on the main frame produces a new
    // iframe which contains the largest content so far.
    if (!IsValid(main_frame_contentful_paint_.Text().Time()))
      Reset(main_frame_contentful_paint_.Text());
    if (!IsValid(main_frame_contentful_paint_.Image().Time()))
      Reset(main_frame_contentful_paint_.Image());
    if (!IsValid(subframe_contentful_paint_.Text().Time()))
      Reset(subframe_contentful_paint_.Text());
    if (!IsValid(subframe_contentful_paint_.Image().Time()))
      Reset(subframe_contentful_paint_.Image());
    if (!IsValid(cross_site_subframe_contentful_paint_.Text().Time()))
      Reset(cross_site_subframe_contentful_paint_.Text());
    if (!IsValid(cross_site_subframe_contentful_paint_.Image().Time()))
      Reset(cross_site_subframe_contentful_paint_.Image());
  }
}

void LargestContentfulPaintHandler::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle,
    base::TimeTicks navigation_start) {
  if (!navigation_handle->HasCommitted())
    return;

  // We have a new committed navigation, so discard information about the
  // previously committed navigation.
  subframe_navigation_start_offset_.erase(
      navigation_handle->GetFrameTreeNodeId());

  if (navigation_start > navigation_handle->NavigationStart())
    return;
  base::TimeDelta navigation_delta;
  // If navigation start offset tracking has been disabled for tests, then
  // record a zero-value navigation delta. Otherwise, compute the actual delta
  // between the main frame navigation start and the subframe navigation start.
  // See crbug/616901 for more details on why navigation start offset tracking
  // is disabled in tests.
  if (!g_disable_subframe_navigation_start_offset) {
    navigation_delta = navigation_handle->NavigationStart() - navigation_start;
  }
  subframe_navigation_start_offset_.insert(std::make_pair(
      navigation_handle->GetFrameTreeNodeId(), navigation_delta));
}

void LargestContentfulPaintHandler::OnSubFrameDeleted(
    content::FrameTreeNodeId frame_tree_node_id) {
  subframe_navigation_start_offset_.erase(frame_tree_node_id);
}

}  // namespace page_load_metrics
