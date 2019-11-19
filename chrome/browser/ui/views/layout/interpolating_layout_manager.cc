// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/layout/interpolating_layout_manager.h"

#include <memory>
#include <utility>

#include "ui/gfx/animation/tween.h"
#include "ui/views/view.h"

InterpolatingLayoutManager::InterpolatingLayoutManager() {}
InterpolatingLayoutManager::~InterpolatingLayoutManager() = default;

InterpolatingLayoutManager& InterpolatingLayoutManager::SetOrientation(
    views::LayoutOrientation orientation) {
  if (orientation_ != orientation) {
    orientation_ = orientation;
    InvalidateHost(true);
  }
  return *this;
}

void InterpolatingLayoutManager::AddLayoutInternal(
    LayoutManagerBase* engine,
    const views::Span& interpolation_range) {
  DCHECK(engine);

  auto result = embedded_layouts_.emplace(
      std::make_pair(interpolation_range, std::move(engine)));
  DCHECK(result.second) << "Cannot replace existing layout manager for "
                        << interpolation_range.ToString();

#if DCHECK_IS_ON()
  // Sanity checking to ensure interpolation ranges do not overlap (we can
  // only interpolate between two layouts currently).
  auto next = result.first;
  ++next;
  if (next != embedded_layouts_.end())
    DCHECK_GE(next->first.start(), interpolation_range.end());
  if (result.first != embedded_layouts_.begin()) {
    auto prev = result.first;
    --prev;
    DCHECK_LE(prev->first.end(), interpolation_range.start());
  }
#endif  // DCHECK_IS_ON()
}

InterpolatingLayoutManager::LayoutInterpolation
InterpolatingLayoutManager::GetInterpolation(
    const views::SizeBounds& size_bounds) const {
  DCHECK(!embedded_layouts_.empty());

  LayoutInterpolation result;

  const base::Optional<int> dimension =
      orientation_ == views::LayoutOrientation::kHorizontal
          ? size_bounds.width()
          : size_bounds.height();

  // Find the larger layout that overlaps the target size.
  auto match = dimension ? embedded_layouts_.upper_bound({*dimension, 0})
                         : embedded_layouts_.end();
  DCHECK(match != embedded_layouts_.begin())
      << "No layout set for primary dimension size "
      << (dimension ? *dimension : -1) << "; first layout starts at "
      << match->first.ToString();
  result.first = (--match)->second;

  // If the target size falls in an interpolation range, get the other layout.
  const views::Span& first_span = match->first;
  if (dimension && first_span.end() > *dimension) {
    DCHECK(match != embedded_layouts_.begin())
        << "Primary dimension size " << (dimension ? *dimension : -1)
        << " falls into interpolation range " << match->first.ToString()
        << " but there is no smaller layout to interpolate with.";
    result.second = (--match)->second;
    result.percent_second =
        float{first_span.end() - *dimension} / float{first_span.length()};
  }

  return result;
}

views::ProposedLayout InterpolatingLayoutManager::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  // For interpolating layout we will never call this method except for fully-
  // specified sizes.
  DCHECK(size_bounds.width());
  DCHECK(size_bounds.height());
  const gfx::Size size(*size_bounds.width(), *size_bounds.height());

  const LayoutInterpolation interpolation = GetInterpolation(size_bounds);
  const views::ProposedLayout first =
      interpolation.first->GetProposedLayout(size);

  if (!interpolation.second)
    return first;

  // If the target size falls in an interpolation range, get the other layout.
  const views::ProposedLayout second =
      interpolation.second->GetProposedLayout(size);
  return views::ProposedLayoutBetween(interpolation.percent_second, first,
                                      second);
}

void InterpolatingLayoutManager::SetDefaultLayout(
    LayoutManagerBase* default_layout) {
  if (default_layout_ == default_layout)
    return;

  // Make sure we already own the layout.
  DCHECK(embedded_layouts_.end() !=
         std::find_if(
             embedded_layouts_.begin(), embedded_layouts_.end(),
             [=](const auto& pair) { return pair.second == default_layout; }));
  default_layout_ = default_layout;
  InvalidateHost(true);
}

gfx::Size InterpolatingLayoutManager::GetPreferredSize(
    const views::View* host) const {
  DCHECK_EQ(host_view(), host);
  DCHECK(host);
  return GetDefaultLayout()->GetPreferredSize(host);
}

gfx::Size InterpolatingLayoutManager::GetMinimumSize(
    const views::View* host) const {
  DCHECK_EQ(host_view(), host);
  DCHECK(host);
  return GetSmallestLayout()->GetMinimumSize(host);
}

int InterpolatingLayoutManager::GetPreferredHeightForWidth(
    const views::View* host,
    int width) const {
  // It is in general not possible to determine what the correct height-for-
  // width trade-off is while interpolating between two already-generated
  // layouts because the values tend to rely on the behavior of individual child
  // views at specific dimensions.
  //
  // The two reasonable choices are to use the larger of the two values (with
  // the understanding that the height of the view may "pop" at the edge of the
  // interpolation range), or to interpolate between the heights and hope that
  // the result is fairly close to what we would want.
  //
  // We have opted for the second approach because it provides a smoother visual
  // experience; if this doesn't work in practice we can look at other options.

  const LayoutInterpolation interpolation =
      GetInterpolation({width, base::nullopt});
  const int first =
      interpolation.first->GetPreferredHeightForWidth(host, width);
  if (!interpolation.second)
    return first;

  const int second =
      interpolation.second->GetPreferredHeightForWidth(host, width);
  return gfx::Tween::LinearIntValueBetween(interpolation.percent_second, first,
                                           second);
}

const views::LayoutManagerBase* InterpolatingLayoutManager::GetDefaultLayout()
    const {
  DCHECK(!embedded_layouts_.empty());
  return default_layout_ ? default_layout_ : embedded_layouts_.rbegin()->second;
}

const views::LayoutManagerBase* InterpolatingLayoutManager::GetSmallestLayout()
    const {
  DCHECK(!embedded_layouts_.empty());
  return embedded_layouts_.begin()->second;
}
